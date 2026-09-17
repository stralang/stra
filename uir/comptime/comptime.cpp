#include "comptime.hpp"
#include "../analysis/define.hpp"
#include "../literal.hpp"
#include "../types.hpp"
#include "../uir.hpp"
#include "containers.hpp"
#include "define.hpp"
#include <cstdlib>
#include <iostream>

UIRLiteral execute(UIRComptime *state, UIRModule *module, UIRValue *inst) {
  ComptimeStackFrame *frame = state->currentStack();

  switch (inst->kind) {
  case UIRValueKind::LocalVariable: {
    // Don't allocate another field
    size_t *ptr_idx = frame->lookup.get(inst);
    if (ptr_idx != nullptr) {
      return {.lit_type = nullptr};
    }

    // Get Type
    UIRLiteral ty_lit =
        state->getValue(frame, module, inst->local_variable.type);

    // Allocate value
    UIRLiteral *value_lit = frame->add(nullptr);
    value_lit->lit_type = ty_lit._typeid;
    value_lit->kind = UIRLiteralKind::Null;
    // TODO: Default

    // Create pointer
    return {
        .lit_type = state->ctx->type_cache->get({
            .kind = TypeKind::Pointer,
            .child = ty_lit._typeid,
            .is_constant = true,
        }),
        .kind = UIRLiteralKind::Typed,
        .pointer = value_lit,
    };
  }
  case UIRValueKind::Load: {
    UIRLiteral ptr = state->getValue(frame, module, inst->load.ptr);
    return *ptr.pointer;
  }
  case UIRValueKind::Store: {
    UIRLiteral ptr = state->getValue(frame, module, inst->store.ptr);
    UIRLiteral value = state->getValue(frame, module, inst->store.value);
    if (ptr.lit_type->child->is_constant) {
      std::cerr << inst->store.ptr->source_location.line
                << " Cannot store to constant\n";
      std::abort();
    }
    // TODO: Compare types

    *ptr.pointer = value;

    return UIRLiteral{
        .lit_type = state->ctx->type_cache->get({.kind = TypeKind::Void}),
        .kind = UIRLiteralKind::Typed,
    };
  }
  case UIRValueKind::Arg: {
    UIRLiteral ty_lit = state->getValue(frame, module, inst->load.ptr);
    // TODO: Compare types

    UIRLiteral *arg_value = frame->values.get(frame->arg_count);
    frame->arg_count += 1;

    return {
        .lit_type = state->ctx->type_cache->get({
            .kind = TypeKind::Pointer,
            .child = ty_lit._typeid,
            .is_constant = true,
        }),
        .kind = UIRLiteralKind::Typed,
        .pointer = arg_value,
    };
  }
  case UIRValueKind::Call: {
    state->pushStack();

    // Get Function
    UIRValue *function = nullptr;
    if (inst->call.callee->kind == UIRValueKind::Function) {
      function = inst->call.callee;
    } else {
      UIRLiteral fn = state->getValue(frame, module, inst->call.callee);
      assert(fn.kind == UIRLiteralKind::Instruction);
      function = fn.instruction;
    }

    // Inject Arguments
    for (size_t i = 0; i < inst->call.arguments.len; i++) {
      UIRValue *arg = inst->call.arguments.ptr[i];
      UIRLiteral value = state->getValue(frame, module, arg);
      state->currentStack()->inject(value);
    }

    executeProgram(state, module, function->function.blocks.get(0));

    UIRLiteral result = **state->currentStack()->values.back();
    state->popStack();
    return result;
  }
  case UIRValueKind::Index: {
    // TODO: Implement compile-time Indexing
    break;
  }
  case UIRValueKind::BinOp: {
    return executeBinary(state, module, frame, inst);
  }
  case UIRValueKind::Return: {
    UIRLiteral result;
    if (inst->ret.value.isSome()) {
      result = state->getValue(frame, module, inst->ret.value.get());
    } else {
      result.lit_type = state->ctx->type_cache->get({.kind = TypeKind::Void});
      result.kind = UIRLiteralKind::Typed;
    }
    return result;
  }

  case UIRValueKind::Comptime: {
    state->pushStack();
    UIRBlock *entry = inst->comptime.blocks.get(0);
    executeProgram(state, module, entry);

    UIRLiteral result = **state->currentStack()->values.back();
    state->popStack();
    return result;
  }
  case UIRValueKind::TypeOf: {
    return UIRLiteral{
        .lit_type = state->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        .kind = UIRLiteralKind::Typed,
        ._typeid = inst->_typeof->result_type,
    };
  }

  case UIRValueKind::GlobalVariable: {
    return state->getValue(nullptr, module, inst);
  }

  case UIRValueKind::Literal: {
    return inst->literal;
  }
  case UIRValueKind::Function: {
    // Analyse Type
    Type raw_type = {.kind = TypeKind::Function, .is_constant = true};
    raw_type.function.arguments = {
        .ptr = (Type **)state->arena->alloc(sizeof(UIRValue) *
                                            inst->function.parameter_types.len),
        .len = inst->function.parameter_types.len,
    };

    // Analyse Parameters
    for (size_t i = 0; i < inst->function.parameter_types.len; i++) {
      UIRValue *param = inst->function.parameter_types.ptr[i];

      UIRLiteral param_literal = execute(state, module, param);
      raw_type.function.arguments.ptr[i] = param_literal._typeid;
    }

    // Analyse Return Type
    UIRLiteral return_literal =
        execute(state, module, inst->function.return_type);
    raw_type.function.return_type = return_literal._typeid;

    // Get final type
    return {
        .lit_type = state->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        ._typeid = state->ctx->type_cache->get(raw_type),
    };
  }
  case UIRValueKind::Pointer: {
    UIRLiteral lit = state->getValue(frame, module, inst->pointer);
    lit._typeid = state->ctx->type_cache->get(
        {.kind = TypeKind::Pointer, .child = lit._typeid});
    return lit;
  }
  case UIRValueKind::Struct: {
    Type raw_type = {
        .kind = TypeKind::Struct,
        .is_constant = true,
    };
    raw_type._struct.inst = inst;

    Type *struct_type = state->ctx->type_cache->get(raw_type);

    // Fields
    struct_type->_struct.fields = {
        .ptr = (Type **)state->arena->alloc(sizeof(Type *) *
                                            inst->_struct.fields.len),
        .len = inst->_struct.fields.len,
    };
    for (size_t i = 0; i < inst->_struct.fields.len; i++) {
      UIRStruct::Field *field = inst->_struct.fields.ptr + i;
      UIRLiteral type_lit = state->getValue(frame, module, field->type);
      struct_type->_struct.fields.ptr[i] = type_lit._typeid;
    }

    return {
        .lit_type = state->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        .kind = UIRLiteralKind::Typed,
        ._typeid = struct_type,
    };
  }
  case UIRValueKind::Enum: {
    Type raw_type = {
        .kind = TypeKind::Enum,
        .is_constant = true,
    };
    raw_type._enum.inst = inst;

    Type *enum_type = state->ctx->type_cache->get(raw_type);

    // Represent Type
    UIRLiteral repr_lit = state->getValue(frame, module, inst->_enum.repr_type);
    enum_type->_enum.repr_type = repr_lit._typeid;

    // Members
    int64_t next_value = 0;
    for (size_t i = 0; i < inst->_enum.members.len; i++) {
      UIREnum::Member *member = inst->_enum.members.ptr + i;

      UIRLiteral result;
      if (member->constant != nullptr) {
        result = execute(state, module, member->constant);
      } else {
        module->instructions.push({});
        member->constant = module->instructions.back();
        result.kind = UIRLiteralKind::Typed;
        result._int = next_value;
      }

      result.lit_type = repr_lit._typeid;
      member->constant->literal = result;
      member->constant->kind = UIRValueKind::Literal;
      member->constant->result_type = member->constant->literal.lit_type;
      next_value = member->constant->literal._int + 1;
    }

    return {
        .lit_type = state->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        .kind = UIRLiteralKind::Typed,
        ._typeid = enum_type,
    };
  }
  case UIRValueKind::Union: {
    Type raw_type = {
        .kind = TypeKind::Union,
        .is_constant = true,
    };
    raw_type._union.inst = inst;

    Type *union_type = state->ctx->type_cache->get(raw_type);

    // Represent Type
    UIRLiteral repr_lit =
        state->getValue(frame, module, inst->_union.repr_type);
    union_type->_enum.repr_type = repr_lit._typeid;

    // Fields
    union_type->_union.variants = {
        .ptr = (Type **)state->arena->alloc(sizeof(Type *) *
                                            inst->_struct.fields.len),
        .len = inst->_struct.fields.len,
    };
    for (size_t i = 0; i < inst->_struct.fields.len; i++) {
      UIRStruct::Field *field = inst->_struct.fields.ptr + i;
      UIRLiteral type_lit = state->getValue(frame, module, field->type);
      union_type->_union.variants.ptr[i] = type_lit._typeid;
    }

    return {
        .lit_type = state->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        .kind = UIRLiteralKind::Typed,
        ._typeid = union_type,
    };
  }
  case UIRValueKind::Namespace: {
    Type raw_type = {.kind = TypeKind::Namespace};
    raw_type._namespace.inst = inst;

    Type *namespace_type = state->ctx->type_cache->get(raw_type);
    return {
        .lit_type = state->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        .kind = UIRLiteralKind::Typed,
        ._typeid = namespace_type,
    };
  }
  case UIRValueKind::Slice: {
    Type raw_type = {.kind = TypeKind::Slice, .is_constant = false};

    UIRLiteral element = state->getValue(frame, module, inst->slice.element);
    assert(element.lit_type->kind == TypeKind::TypeId);
    raw_type.slice.type = element._typeid;

    if (inst->slice.is_pointer) {
      raw_type.slice.length = -1;
    } else if (inst->slice.length != nullptr) {
      UIRLiteral length = state->getValue(frame, module, inst->slice.length);
      assert(length.lit_type->kind == TypeKind::Integer &&
             (length.lit_type->integer.is_untyped ||
              (length.lit_type->integer.bits =
                   -1 && !length.lit_type->integer.is_signed)));
      raw_type.slice.length = length._int;
    } else {
      raw_type.slice.length = 0;
    }

    return {
        .lit_type = state->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        ._typeid = state->ctx->type_cache->get(raw_type),
    };
  }
  }

  std::cerr << "TODO: Implement compile-time execution of `" << std::hex
            << (uint16_t)inst->kind << "`\n";
  std::abort();
}

void executeProgram(UIRComptime *state, UIRModule *module,
                    UIRBlock *entrypoint) {
  ComptimeStackFrame *frame = state->currentStack();
  ArrayList<UIRValue *> *program = &entrypoint->instructions;
  size_t pc = 0;
  while (pc < program->length) {
    UIRValue *inst = program->getUnchecked(pc);
    pc += 1;

    // Branch
    if (inst->kind == UIRValueKind::Branch) {
      pc = 0;
      program = &inst->br->instructions;
      continue;
    } else if (inst->kind == UIRValueKind::CondBranch) {
      pc = 0;

      UIRLiteral cond = state->getValue(frame, module, inst->condbr.condition);
      assert(cond.lit_type->kind == TypeKind::Bool &&
             "Condition was not boolean");

      if (cond._bool) {
        program = &inst->condbr.then->instructions;
      } else {
        program = &inst->condbr._else->instructions;
      }
      continue;
    }

    // Execute instruction
    UIRLiteral value = execute(state, module, inst);
    if (value.lit_type != nullptr) {
      *frame->add(inst) = value;
    }
  }
}

UIRLiteral UIRComptime::execute(UIRModule *module, UIRValue *inst) {
  this->pushStack();
  UIRLiteral result = ::execute(this, module, inst);
  this->popStack();

  return result;
}

void UIRComptime::init(Allocator *allocator, DynamicArena *arena) {
  this->allocator = allocator;
  this->call_stack.init(allocator, 8);
  this->arena = arena;
}
void UIRComptime::deinit() { this->call_stack.deinit(); }

void UIRComptime::pushStack() {
  ComptimeStackFrame frame;
  frame.lookup.init(this->allocator, 32);
  frame.values.init(this->allocator, 32);
  frame.arena.init(this->allocator, 1024 * 1024);
  this->call_stack.push(frame);
}

void UIRComptime::popStack() {
  ComptimeStackFrame frame = this->call_stack.pop();
  frame.lookup.deinit();
  frame.values.deinit();
  frame.arena.deinit();
}

ComptimeStackFrame *UIRComptime::currentStack() {
  return this->call_stack.back();
}

UIRLiteral UIRComptime::getValue(ComptimeStackFrame *frame, UIRModule *module,
                                 UIRValue *from) {
  if (from->kind == UIRValueKind::Literal) {
    return from->literal;
  } else if (from->kind == UIRValueKind::GlobalVariable) {
    UIRValue *constant = from->global_variable.constant.get();
    if (constant->kind != UIRValueKind::Literal) {
      analyseGlobal(this->analyser, module, from);
    }

    // Make the literal type constant
    // because all global variables are constant during compile-time execution
    Type vtype = *constant->result_type;
    vtype.is_constant = true;

    Type *real_vtype = this->ctx->type_cache->get(vtype);
    Type *ptr_type = this->ctx->type_cache->get({
        .kind = TypeKind::Pointer,
        .child = real_vtype,
        .is_constant = true,
    });

    // Return Pointer to constant
    return UIRLiteral{
        .lit_type = ptr_type,
        .pointer = &constant->literal,
    };
  }

  size_t idx = *frame->lookup.get(from);
  return *frame->values.getUnchecked(idx);
}
