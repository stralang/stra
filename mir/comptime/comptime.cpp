#include "comptime.hpp"
#include "analysis/define.hpp"
#include "containers.hpp"
#include "define.hpp"
#include "literal.hpp"
#include "mir.hpp"
#include "types.hpp"
#include <cstdlib>
#include <iostream>

MIRLiteral execute(MIRComptime *state, MIRModule *module, MIRValue *inst) {
  ComptimeStackFrame *frame = state->currentStack();

  switch (inst->kind) {
  case MIRValueKind::Alloca: {
    // Don't allocate another field
    size_t *ptr_idx = frame->lookup.get(inst);
    if (ptr_idx != nullptr) {
      return {.lit_type = nullptr};
    }

    // Get Type
    MIRLiteral ty_lit = *state->getValue(frame, module, inst->alloca.type);

    // Allocate value
    MIRLiteral *value_lit = frame->add(nullptr);
    value_lit->lit_type = ty_lit._typeid;
    value_lit->kind = MIRLiteralKind::Null;
    // TODO: Default

    // Create pointer
    return {
        .lit_type = module->ctx->type_cache->get({
            .kind = TypeKind::Pointer,
            .child = ty_lit._typeid,
            .is_constant = true,
        }),
        .kind = MIRLiteralKind::Typed,
        .pointer = value_lit,
    };
  }
  case MIRValueKind::Load: {
    MIRLiteral ptr = *state->getValue(frame, module, inst->load.ptr);
    return *ptr.pointer;
  }
  case MIRValueKind::Store: {
    MIRLiteral ptr = *state->getValue(frame, module, inst->store.ptr);
    MIRLiteral value = *state->getValue(frame, module, inst->store.value);
    // TODO: Compare types

    *ptr.pointer = value;

    return MIRLiteral{
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::Void}),
        .kind = MIRLiteralKind::Typed,
    };
  }
  case MIRValueKind::Arg: {
    MIRLiteral ty_lit = *state->getValue(frame, module, inst->arg.type);
    // TODO: Compare types

    MIRLiteral *arg_value = frame->values.get(frame->arg_count);
    frame->arg_count += 1;

    return {
        .lit_type = module->ctx->type_cache->get({
            .kind = TypeKind::Pointer,
            .child = ty_lit._typeid,
            .is_constant = true,
        }),
        .kind = MIRLiteralKind::Typed,
        .pointer = arg_value,
    };
  }
  case MIRValueKind::Call: {
    state->pushStack();

    // Get Function
    MIRValue *function = module->getInstr(inst->call.callee);
    if (function->kind != MIRValueKind::Function) {
      MIRLiteral *fn = state->getValue(frame, module, inst->call.callee);
      assert(fn->kind == MIRLiteralKind::Instruction);
      function = fn->instruction;
    }

    // Inject Arguments
    for (size_t i = 0; i < inst->call.arguments.len; i++) {
      MIRLiteral *value =
          state->getValue(frame, module, inst->call.arguments.ptr[i]);
      state->currentStack()->values.push(value);
    }

    MIRBlock *entry_block = module->getBlock(function->function.blocks.get(0));
    executeProgram(state, module, entry_block);

    MIRLiteral result = **state->currentStack()->values.back();
    state->popStack();
    return result;
  }
  case MIRValueKind::GEP: {
    // TODO: Implement compile-time GEP
    break;
  }
  case MIRValueKind::BinOp: {
    return executeBinary(state, module, frame, inst);
  }
  case MIRValueKind::Return: {
    MIRLiteral result;
    if (inst->ret.value.isSome()) {
      result = *state->getValue(frame, module, inst->ret.value.get());
    } else {
      result.lit_type = module->ctx->type_cache->get({.kind = TypeKind::Void});
      result.kind = MIRLiteralKind::Typed;
    }
    return result;
  }

  case MIRValueKind::Comptime: {
    state->pushStack();
    MIRBlock *entry = module->getBlock(inst->comptime.blocks.get(0));
    executeProgram(state, module, entry);

    MIRLiteral result = **state->currentStack()->values.back();
    state->popStack();
    return result;
  }
  case MIRValueKind::TypeOf: {
    MIRValue *_typeof_inst = module->getInstr(inst->_typeof);
    return MIRLiteral{
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        .kind = MIRLiteralKind::Typed,
        ._typeid = _typeof_inst->result_type,
    };
  }

  case MIRValueKind::GlobalVariable: {
    return executeGlobalVariable(state, module, frame, inst);
  }

  case MIRValueKind::Literal: {
    return inst->literal;
  }
  case MIRValueKind::Function: {
    // Analyse Type
    Type raw_type = {.kind = TypeKind::Function, .is_constant = true};
    raw_type.function.arguments = {
        .ptr = (Type **)state->arena->alloc(sizeof(MIRValue) *
                                            inst->function.parameter_types.len),
        .len = inst->function.parameter_types.len,
    };

    // Analyse Parameters
    for (size_t i = 0; i < inst->function.parameter_types.len; i++) {
      MIRValue *param = module->getInstr(inst->function.parameter_types.ptr[i]);
      MIRLiteral param_literal = execute(state, module, param);
      raw_type.function.arguments.ptr[i] = param_literal._typeid;
    }

    // Analyse Return Type
    MIRValue *return_type_inst =
        module->getInstr(inst->function.return_type.get());
    MIRLiteral return_literal = execute(state, module, return_type_inst);
    raw_type.function.return_type = return_literal._typeid;

    // Get final type
    return {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        ._typeid = module->ctx->type_cache->get(raw_type),
    };
  }
  case MIRValueKind::Struct: {
    Type raw_type = {
        .kind = TypeKind::Struct,
        .is_constant = true,
    };
    raw_type._struct.inst = inst;

    Type *struct_type = module->ctx->type_cache->get(raw_type);

    // Fields
    struct_type->_struct.fields = {
        .ptr = (Type **)state->arena->alloc(sizeof(Type *) *
                                            inst->_struct.fields.len),
        .len = inst->_struct.fields.len,
    };
    for (size_t i = 0; i < inst->_struct.fields.len; i++) {
      MIRStruct::Field *field = inst->_struct.fields.ptr + i;
      MIRLiteral type_lit = *state->getValue(frame, module, field->type);
      struct_type->_struct.fields.ptr[i] = type_lit._typeid;
    }

    return {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        .kind = MIRLiteralKind::Typed,
        ._typeid = struct_type,
    };
  }
  case MIRValueKind::Enum: {
    Type raw_type = {
        .kind = TypeKind::Enum,
        .is_constant = true,
    };
    raw_type._enum.inst = inst;

    Type *enum_type = module->ctx->type_cache->get(raw_type);

    // Represent Type
    MIRLiteral repr_lit =
        *state->getValue(frame, module, inst->_enum.repr_type);
    enum_type->_enum.repr_type = repr_lit._typeid;

    return {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        .kind = MIRLiteralKind::Typed,
        ._typeid = enum_type,
    };
  }
  case MIRValueKind::Union: {
    Type raw_type = {
        .kind = TypeKind::Union,
        .is_constant = true,
    };
    raw_type._union.inst = inst;

    Type *union_type = module->ctx->type_cache->get(raw_type);

    // Represent Type
    MIRLiteral repr_lit =
        *state->getValue(frame, module, inst->_union.repr_type);
    union_type->_enum.repr_type = repr_lit._typeid;

    // Fields
    union_type->_union.variants = {
        .ptr = (Type **)state->arena->alloc(sizeof(Type *) *
                                            inst->_struct.fields.len),
        .len = inst->_struct.fields.len,
    };
    for (size_t i = 0; i < inst->_struct.fields.len; i++) {
      MIRStruct::Field *field = inst->_struct.fields.ptr + i;
      MIRLiteral type_lit = *state->getValue(frame, module, field->type);
      union_type->_union.variants.ptr[i] = type_lit._typeid;
    }

    return {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        .kind = MIRLiteralKind::Typed,
        ._typeid = union_type,
    };
  }
  case MIRValueKind::Namespace: {
    Type raw_type = {.kind = TypeKind::Namespace};
    raw_type._namespace.inst = inst;

    Type *namespace_type = module->ctx->type_cache->get(raw_type);
    return {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        .kind = MIRLiteralKind::Typed,
        ._typeid = namespace_type,
    };
  }
  case MIRValueKind::Slice: {
    Type raw_type = {.kind = TypeKind::Slice, .is_constant = true};

    MIRLiteral element = *state->getValue(frame, module, inst->slice.element);
    assert(element.lit_type->kind == TypeKind::TypeId);
    raw_type.slice.type = element._typeid;

    if (inst->slice.is_pointer) {
      raw_type.slice.length = -1;
    } else if (inst->slice.length.isSome()) {
      MIRLiteral length =
          *state->getValue(frame, module, inst->slice.length.get());
      assert(length.lit_type->kind == TypeKind::Integer &&
             (length.lit_type->integer.is_untyped ||
              (length.lit_type->integer.bits =
                   -1 && !length.lit_type->integer.is_signed)));
      raw_type.slice.length = length._int;
    } else {
      raw_type.slice.length = 0;
    }

    return {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        ._typeid = module->ctx->type_cache->get(raw_type),
    };
  }
  }

  std::cerr << "TODO: Implement compile-time execution of `" << std::hex
            << (uint16_t)inst->kind << "`\n";
  std::abort();
}

void executeProgram(MIRComptime *state, MIRModule *module,
                    MIRBlock *entrypoint) {
  ComptimeStackFrame *frame = state->currentStack();
  ArrayList<MIRValueId> *program = &entrypoint->instructions;
  size_t pc = 0;
  while (pc < program->length) {
    MIRValue *inst = module->getInstr(program->getUnchecked(pc));
    pc += 1;

    // Branch
    if (inst->kind == MIRValueKind::Branch) {
      pc = 0;
      program = &module->getBlock(inst->br)->instructions;
      continue;
    } else if (inst->kind == MIRValueKind::CondBranch) {
      pc = 0;

      MIRLiteral cond = *state->getValue(frame, module, inst->condbr.condition);
      assert(cond.lit_type->kind == TypeKind::Bool &&
             "Condition was not boolean");

      if (cond._bool) {
        program = &module->getBlock(inst->condbr.then)->instructions;
      } else {
        program = &module->getBlock(inst->condbr._else)->instructions;
      }
      continue;
    }

    // Execute instruction
    MIRLiteral value = execute(state, module, inst);
    if (value.lit_type != nullptr) {
      *frame->add(inst) = value;
    }
  }
}

MIRLiteral MIRComptime::execute(MIRModule *module, MIRValue *inst) {
  this->pushStack();
  MIRLiteral result = ::execute(this, module, inst);
  this->popStack();

  return result;
}

void MIRComptime::init(Allocator *allocator, DynamicArena *arena) {
  this->allocator = allocator;
  this->call_stack.init(allocator, 8);
  this->globals.init(allocator, 32);
  this->arena = arena;
}
void MIRComptime::deinit() {
  this->call_stack.deinit();
  this->globals.deinit();
}

void MIRComptime::pushStack() {
  ComptimeStackFrame frame;
  frame.lookup.init(this->allocator, 32);
  frame.values.init(this->allocator, 32);
  frame.arena.init(this->allocator, 1024 * 1024);
  this->call_stack.push(frame);
}

void MIRComptime::popStack() {
  ComptimeStackFrame frame = this->call_stack.pop();
  frame.lookup.deinit();
  frame.values.deinit();
  frame.arena.deinit();
}

ComptimeStackFrame *MIRComptime::currentStack() {
  return this->call_stack.back();
}

MIRLiteral *MIRComptime::getValue(ComptimeStackFrame *frame, MIRModule *module,
                                  MIRValueId from) {
  MIRValue *from_inst = module->getInstr(from);
  if (from_inst->kind == MIRValueKind::Literal) {
    return &from_inst->literal;
  } else if (from_inst->kind == MIRValueKind::GlobalVariable) {
    MIRLiteral **global = this->globals.get(from_inst);
    if (global == nullptr) {
      this->execute(module, from_inst);
      global = this->globals.get(from_inst);
    }

    return *global;
  }

  size_t idx = *frame->lookup.get(from_inst);
  return frame->values.getUnchecked(idx);
}

void MIRComptime::foldGlobals() {
  for (size_t i = 0; i < this->globals.slot_capacity; i++) {
    auto slot = this->globals.slots + i;
    if (!slot->alive) {
      continue;
    }

    MIRValue *constant = this->analyser->ctx->getInstr(
        slot->key->global_variable.constant.get());
    constant->kind = MIRValueKind::Literal;
    constant->literal = *slot->value->pointer;
  }
}
