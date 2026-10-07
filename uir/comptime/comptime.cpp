#include "comptime.hpp"
#include "../analysis/define.hpp"
#include "../literal.hpp"
#include "../types.hpp"
#include "../uir.hpp"
#include "common/debug.hpp"
#include "containers.hpp"
#include "define.hpp"
#include "rir/constant.hpp"
#include "rir/type.hpp"
#include "uir/analysis/analysis.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>

UIRRawData convertRIRConstant(RIRConstant constant) {
  UIRRawData data_out;
  switch (constant.kind) {
  case RIRConstantKind::Bool: {
    data_out.kind = UIRRawDataKind::Bool;
    data_out._bool = constant._bool;
    break;
  }
  case RIRConstantKind::Integer: {
    data_out.kind = UIRRawDataKind::Int;
    data_out._int = constant.integer;
    break;
  }
  case RIRConstantKind::Float: {
    data_out.kind = UIRRawDataKind::Float;
    data_out._float = constant._float;
    break;
  }
  }
  return data_out;
}

void execute(UIRComptime *state, UIRValue *inst) {
  ComptimeStackFrame *frame = state->currentStack();

  switch (inst->kind) {
  case UIRValueKind::LocalVariable: {
    // Don't allocate another field
    size_t *ptr_idx = frame->lookup.get(inst->id);
    if (ptr_idx != nullptr) {
      return;
    }

    // Get Type
    UIRLiteral ty_lit = state->getValue(frame, inst->local_variable.type);
    // TODO: Confirm `ty_lit` is of `UIRLiteralKind::TypeId`

    // Allocate value
    UIRLiteral *value_lit = frame->add({});
    value_lit->lit_type = ty_lit.data._typeid;

    RIRType *real_type = state->analyser->rir_ctx->getType(ty_lit.data._typeid);
    switch (real_type->kind) {
    case RIRTypeKind::Bool: {
      value_lit->data.kind = UIRRawDataKind::Bool;
      break;
    }
    case RIRTypeKind::Integer: {
      value_lit->data.kind = UIRRawDataKind::Int;
      break;
    }
    case RIRTypeKind::Float: {
      value_lit->data.kind = UIRRawDataKind::Float;
      break;
    }
    case RIRTypeKind::Pointer: {
      value_lit->data.kind = UIRRawDataKind::Pointer;
      break;
    }
    case RIRTypeKind::Slice: {
      value_lit->data.kind = UIRRawDataKind::Slice;
      break;
    }
    }
    // TODO: Default

    // Create pointer
    UIRLiteral *lit_out = frame->add(inst->id);
    lit_out->lit_type = state->analyser->rir_ctx->types->push({
        .kind = RIRTypeKind::Pointer,
        .child = ty_lit.data._typeid,
        // TODO: Readd constant types
    });
    lit_out->data.kind = UIRRawDataKind::Pointer;
    lit_out->data.ptr = {.kind = UIRPlaceKind::Raw, .data = &value_lit->data};
    return;
  }
  case UIRValueKind::Load: {
    UIRLiteral ptr = state->getValue(frame, inst->load.ptr);
    assert(ptr.data.kind == UIRRawDataKind::Pointer);

    // Create result
    UIRLiteral *lit_out = frame->add(inst->id);
    if (ptr.lit_type.isSome()) {
      RIRType *real_type =
          state->analyser->rir_ctx->getType(ptr.lit_type.get());
      lit_out->lit_type = real_type->child;
    }

    if (ptr.data.ptr.kind == UIRPlaceKind::Inst) {
      RIRValue *rir_inst = state->analyser->rir_ctx->getInst(ptr.data.ptr.inst);
      assert(rir_inst->global_variable.constant.isSome());
      lit_out->data =
          convertRIRConstant(rir_inst->global_variable.constant.get());
    } else if (ptr.data.ptr.kind == UIRPlaceKind::Raw) {
      lit_out->data = *ptr.data.ptr.data;
    }
    return;
  }
  case UIRValueKind::Store: {
    UIRLiteral ptr = state->getValue(frame, inst->store.ptr);
    UIRLiteral value = state->getValue(frame, inst->store.value);

    // TODO: Readd constant types
    // RIRType *ptr_type = state->analyser->rir_ctx->getType(ptr.lit_type);
    // RIRType *child_type = state->analyser->rir_ctx->getType(ptr_type->child);
    // if (child_type->is_constant) {
    //   std::cerr << inst->store.ptr->source_location.line
    //             << " Cannot store to constant\n";
    //   std::abort();
    // }
    // TODO: Compare types

    assert(ptr.data.ptr.kind == UIRPlaceKind::Raw);
    *ptr.data.ptr.data = value.data;
    return;
  }
  case UIRValueKind::Arg: {
    UIRLiteral ty_lit = state->getValue(frame, inst->load.ptr);
    // TODO: Confirm `ty_lit` is of `UIRLiteralKind::TypeId`
    // TODO: Compare types

    UIRLiteral *arg_value = frame->values.get(frame->arg_count);
    frame->arg_count += 1;

    UIRLiteral *lit_out = frame->add(inst->id);
    lit_out->lit_type = state->analyser->rir_ctx->types->push({
        .kind = RIRTypeKind::Pointer,
        .child = ty_lit.data._typeid,
        // TODO: Readd constant types
    });
    lit_out->data.kind = UIRRawDataKind::Pointer;
    lit_out->data.ptr = {
        .kind = UIRPlaceKind::Raw,
        .data = &arg_value->data,
    };
    return;
  }
  case UIRValueKind::BinOp: {
    *(frame->add(inst->id)) = executeBinary(state, frame, inst);
    return;
  }
  case UIRValueKind::Call: {
    state->pushStack();

    // Get Function
    UIRValue *function = nullptr;
    UIRValue *callee_inst = state->ctx->getInst(inst->call.callee);
    if (callee_inst->kind == UIRValueKind::Function) {
      function = callee_inst;
    } else {
      // FIXME:
      // UIRLiteral fn = state->getValue(frame, module, inst->call.callee);
      // assert(fn.kind == UIRLiteralKind::Instruction);
      // function = fn.instruction;
    }

    // Inject Arguments
    for (size_t i = 0; i < inst->call.arguments.len; i++) {
      UIRValueId arg = inst->call.arguments.ptr[i];
      UIRLiteral value = state->getValue(frame, arg);
      state->currentStack()->inject(value);
    }

    executeProgram(state, function->function.blocks.get(0));

    UIRLiteral result = **state->currentStack()->values.back();
    state->popStack();

    *(frame->add(inst->id)) = result;
    return;
  }
  case UIRValueKind::LookupPtr: {
    Option<UIRLiteral> lit = executeLookupPtr(state, frame, inst);
    if (lit.isNone()) {
      // Error
      std::cerr << "Couldn't find member of name \"" << inst->lookup.member
                << "\"\n";
      std::abort();
    }

    *(frame->add(inst->id)) = lit.get();
    return;
  }
  case UIRValueKind::LookupValue: {
    Option<UIRLiteral> lit = executeLookupValue(state, frame, inst);
    if (lit.isNone()) {
      // Error
      std::cerr << "Couldn't find member of name \"" << inst->lookup.member
                << "\"\n";
      std::abort();
    }

    *(frame->add(inst->id)) = lit.get();
    return;
  }
  case UIRValueKind::Return: {
    UIRLiteral result;
    if (inst->ret.value.isSome()) {
      result = state->getValue(frame, inst->ret.value.get());
    } else {
      result.lit_type =
          state->analyser->rir_ctx->types->push({.kind = RIRTypeKind::Void});
      result.data.kind = UIRRawDataKind::Void;
    }

    *(frame->add(inst->id)) = result;
    return;
  }

  case UIRValueKind::Comptime: {
    state->pushStack();
    executeProgram(state, inst->comptime.blocks.get(0));

    UIRLiteral result = **state->currentStack()->values.back();
    state->popStack();

    *(frame->add(inst->id)) = result;
    return;
  }
  case UIRValueKind::TypeOf: {
    UIRLiteral *lit_out = frame->add(inst->id);
    lit_out->data.kind = UIRRawDataKind::TypeId;

    UIRResolved *resolved_inst =
        state->analyser->resolved_mapping.get(inst->_typeof);
    if (resolved_inst == nullptr) {
      UIRLiteral comptime_result = state->getValue(frame, inst->_typeof);
      lit_out->data._typeid = comptime_result.lit_type.get();
      return;
    }

    // Get type from instruction
    if (resolved_inst->kind == UIRResolvedKind::Inst) {
      RIRValue *rir_inst =
          state->analyser->rir_ctx->getInst(resolved_inst->inst);
      lit_out->data._typeid = rir_inst->result;
      return;
    }

    // Get type from literal
    assert(resolved_inst->kind == UIRResolvedKind::Literal);

    if (resolved_inst->literal.lit_type.isSome()) {
      lit_out->data._typeid = resolved_inst->literal.lit_type.get();
    } else {
      // Guess type from data
      switch (resolved_inst->literal.data.kind) {
      case UIRRawDataKind::Void: {
        lit_out->data._typeid =
            state->ctx->types->push({.kind = RIRTypeKind::Void});
        break;
      }
      case UIRRawDataKind::Bool: {
        lit_out->data._typeid =
            state->ctx->types->push({.kind = RIRTypeKind::Bool});
        break;
      }
      case UIRRawDataKind::Int: {
        lit_out->data._typeid = state->ctx->types->push(
            {.kind = RIRTypeKind::Integer, .integer = {true, 32}});
        break;
      }
      case UIRRawDataKind::Float: {
        lit_out->data._typeid = state->ctx->types->push(
            {.kind = RIRTypeKind::Float, .float_bits = 32});
        break;
      }
      case UIRRawDataKind::TypeId: {
        lit_out->data._typeid =
            state->ctx->types->push({.kind = RIRTypeKind::TypeId});
        break;
      }
      }
    }
    return;
  }

  case UIRValueKind::GlobalVariable: {
    *(frame->add(inst->id)) = state->getValue(nullptr, inst->id);
    return;
  }

  case UIRValueKind::Literal: {
    *(frame->add(inst->id)) = inst->literal;
    return;
  }
  case UIRValueKind::Function: {
    // Analyse Type
    RIRType raw_type = {
        .kind = RIRTypeKind::Function}; // TODO: Readd constant types
    raw_type.function.arguments = {
        .ptr = (RIRTypeId *)state->arena->alloc(
            sizeof(RIRTypeId) * inst->function.parameter_types.len),
        .len = inst->function.parameter_types.len,
    };

    // Analyse Parameters
    for (size_t i = 0; i < inst->function.parameter_types.len; i++) {
      UIRValueId param = inst->function.parameter_types.ptr[i];

      UIRLiteral *param_literal =
          *executeGetReturn(state, state->ctx->getInst(param));
      assert(param_literal->data.kind == UIRRawDataKind::TypeId);
      raw_type.function.arguments.ptr[i] = param_literal->data._typeid;
    }

    // Analyse Return Type
    UIRLiteral *return_literal = *executeGetReturn(
        state, state->ctx->getInst(inst->function.return_type));
    raw_type.function._return = return_literal->data._typeid;

    // Get final type
    UIRLiteral *lit_out = frame->add(inst->id);
    lit_out->data.kind = UIRRawDataKind::TypeId;
    lit_out->data._typeid = state->analyser->rir_ctx->types->push(raw_type);
    return;
  }
  case UIRValueKind::Pointer: {
    UIRLiteral lit = state->getValue(frame, inst->pointer);

    UIRLiteral *lit_out = frame->add(inst->id);
    lit_out->data.kind = UIRRawDataKind::TypeId;
    lit_out->data._typeid = state->analyser->rir_ctx->types->push(
        {.kind = RIRTypeKind::Pointer, .child = lit.data._typeid});
    return;
  }
  case UIRValueKind::Struct: {
    RIRType raw_type = {
        .kind = RIRTypeKind::Struct,
        // TODO: Readd constant types
    };
    RIRTypeId struct_type_id = state->analyser->rir_ctx->types->push(raw_type);
    state->analyser->type_extras.insert(struct_type_id, {.creator = inst});

    // Fields
    RIRType *struct_type = state->analyser->rir_ctx->getType(struct_type_id);
    struct_type->_struct.fields = {
        .ptr = (RIRTypeId *)state->arena->alloc(sizeof(RIRTypeId) *
                                                inst->_struct.fields.len),
        .len = inst->_struct.fields.len,
    };
    for (size_t i = 0; i < inst->_struct.fields.len; i++) {
      UIRStruct::Field *field = inst->_struct.fields.ptr + i;
      UIRLiteral type_lit = state->getValue(frame, field->type);
      assert(type_lit.data.kind == UIRRawDataKind::TypeId);
      struct_type->_struct.fields.ptr[i] = type_lit.data._typeid;
    }

    UIRLiteral *lit_out = frame->add(inst->id);
    lit_out->data = {.kind = UIRRawDataKind::TypeId, ._typeid = struct_type_id};
    return;
  }
  case UIRValueKind::Enum: {
    RIRTypeId enum_type_id =
        state->analyser->rir_ctx->types->push({.kind = RIRTypeKind::Enum});
    state->analyser->type_extras.insert(enum_type_id, {.creator = inst});

    // Represent Type
    RIRType *enum_type = state->analyser->rir_ctx->getType(enum_type_id);
    UIRLiteral repr_lit = state->getValue(frame, inst->_enum.repr_type);
    enum_type->_enum.repr = repr_lit.data._typeid;

    // Members
    Slice<UIRLiteral> members = {
        .ptr = (UIRLiteral *)state->allocator->alloc(sizeof(UIRLiteral) *
                                                     inst->_enum.members.len),
        .len = inst->_enum.members.len,
    };
    int64_t next_value = 0;
    for (size_t i = 0; i < inst->_enum.members.len; i++) {
      UIREnum::Member *member = inst->_enum.members.ptr + i;

      UIRLiteral *result = members.ptr + i;
      if (member->constant.isSome()) {
        *result = **executeGetReturn(
            state, state->ctx->getInst(member->constant.get()));
      } else {
        result->data = {.kind = UIRRawDataKind::Int, ._int = next_value};
      }

      // TODO: verify `result` is correct type
      result->lit_type = repr_lit.data._typeid;
      next_value = result->data._int + 1;
    }

    state->analyser->type_extras.get(enum_type_id)->constants = members;

    UIRLiteral *lit_out = frame->add(inst->id);
    lit_out->data = {.kind = UIRRawDataKind::TypeId, ._typeid = enum_type_id};
    return;
  }
  case UIRValueKind::Union: {
    RIRType raw_type = {
        .kind = RIRTypeKind::Union,
        // TODO: Readd constant types
    };
    RIRTypeId union_type_id = state->analyser->rir_ctx->types->push(raw_type);
    state->analyser->type_extras.insert(union_type_id, {.creator = inst});

    // Represent Type
    RIRType *union_type = state->analyser->rir_ctx->getType(union_type_id);
    UIRLiteral repr_lit = state->getValue(frame, inst->_union.repr_type);
    assert(repr_lit.data.kind == UIRRawDataKind::TypeId);
    union_type->_enum.repr = repr_lit.data._typeid;

    // Fields
    union_type->_union.variants = {
        .ptr = (RIRTypeId *)state->arena->alloc(sizeof(RIRTypeId) *
                                                inst->_struct.fields.len),
        .len = inst->_struct.fields.len,
    };
    for (size_t i = 0; i < inst->_struct.fields.len; i++) {
      UIRStruct::Field *field = inst->_struct.fields.ptr + i;
      UIRLiteral type_lit = state->getValue(frame, field->type);
      assert(type_lit.data.kind = UIRRawDataKind::TypeId);
      union_type->_union.variants.ptr[i] = type_lit.data._typeid;
    }

    UIRLiteral *lit_out = frame->add(inst->id);
    lit_out->data = {.kind = UIRRawDataKind::TypeId, ._typeid = union_type_id};
    return;
  }
  case UIRValueKind::Namespace: {
    UIRLiteral *lit_out = frame->add(inst->id);
    lit_out->data = {.kind = UIRRawDataKind::Namespace, ._namespace = inst};
    return;
    // Type raw_type = {.kind = TypeKind::Namespace};
    // raw_type._namespace.inst = inst;
    //
    // Type *namespace_type = state->ctx->type_cache->get(raw_type);
    // return {
    //     .lit_type = state->ctx->type_cache->get({.kind = TypeKind::TypeId}),
    //     .kind = UIRLiteralKind::Typed,
    //     ._typeid = namespace_type,
    // };
  }
  case UIRValueKind::Slice: {
    RIRType raw_type = {.kind =
                            RIRTypeKind::Slice}; // TODO: Readd constant types

    UIRLiteral element = state->getValue(frame, inst->slice.element);
    assert(element.data.kind == UIRRawDataKind::TypeId);
    raw_type.slice.child = element.data._typeid;

    if (inst->slice.is_pointer) {
      raw_type.slice.length = -1;
    } else if (inst->slice.length.isSome()) {
      UIRLiteral length = state->getValue(frame, inst->slice.length.get());

      assert(length.data.kind = UIRRawDataKind::Int);
      if (length.lit_type.isSome()) {
        RIRType *length_type =
            state->analyser->rir_ctx->getType(length.lit_type.get());
        assert(length_type->integer.bits =
                   -1 && !length_type->integer.is_signed);
      }

      raw_type.slice.length = length.data._int;
    } else {
      raw_type.slice.length = 0;
    }

    UIRLiteral *lit_out = frame->add(inst->id);
    lit_out->data = {.kind = UIRRawDataKind::TypeId,
                     ._typeid =
                         state->analyser->rir_ctx->types->push(raw_type)};
    return;
  }
  }

  std::cerr << "TODO: Implement compile-time execution of `" << std::hex
            << (uint16_t)inst->kind << "`\n";
  std::abort();
}

void executeProgram(UIRComptime *state, UIRBlockId entrypoint_id) {
  UIRBlock *entrypoint = state->ctx->getBlock(entrypoint_id);
  ComptimeStackFrame *frame = state->currentStack();
  ArrayList<UIRValueId> *program = &entrypoint->instructions;
  size_t pc = 0;
  while (pc < program->length) {
    UIRValue *inst = state->ctx->getInst(program->getUnchecked(pc));
    pc += 1;

    // Branch
    if (inst->kind == UIRValueKind::Branch) {
      pc = 0;
      program = &state->ctx->getBlock(inst->br)->instructions;
      continue;
    } else if (inst->kind == UIRValueKind::CondBranch) {
      pc = 0;

      UIRLiteral cond = state->getValue(frame, inst->condbr.condition);
      assert(cond.data.kind == UIRRawDataKind::Bool &&
             "Condition was not boolean");

      if (cond.data._bool) {
        program = &state->ctx->getBlock(inst->condbr.then)->instructions;
      } else {
        program = &state->ctx->getBlock(inst->condbr._else)->instructions;
      }
      continue;
    }

    // Execute instruction
    execute(state, inst);
  }
}

UIRLiteral **executeGetReturn(UIRComptime *state, UIRValue *inst) {
  execute(state, inst);
  return state->currentStack()->values.back();
}

UIRLiteral UIRComptime::execute(UIRValue *inst) {
  this->pushStack();
  UIRLiteral result = **executeGetReturn(this, inst);
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

UIRLiteral UIRComptime::getValue(ComptimeStackFrame *frame, UIRValueId from) {
  size_t *opt_idx = frame->lookup.get(from);
  if (opt_idx != nullptr) {
    return *frame->values.getUnchecked(*opt_idx);
  }

  UIRValue *from_inst = this->ctx->getInst(from);
  UIRResolved *resolved_value = this->analyser->resolved_mapping.get(from);
  if (resolved_value == nullptr) {
    analyse(this->analyser, from_inst);
    resolved_value = this->analyser->resolved_mapping.get(from);
  }

  if (resolved_value->kind == UIRResolvedKind::Literal) {
    return resolved_value->literal;
  } else if (from_inst->kind == UIRValueKind::GlobalVariable) {
    UIRValueId constant = from_inst->global_variable.constant.get();
    if (resolved_value == nullptr) {
      analyseGlobal(this->analyser, from_inst);
      resolved_value = this->analyser->resolved_mapping.get(constant);
    }

    RIRTypeId type_id;
    if (resolved_value->kind == UIRResolvedKind::Inst) {
      type_id = this->analyser->rir_ctx->getInst(resolved_value->inst)->result;
    } else if (resolved_value->kind == UIRResolvedKind::Literal) {
      type_id = resolved_value->literal.lit_type.get();
    }

    // TODO: Readd constant types
    // // Make the literal type constant
    // // because all global variables are constant during compile-time
    // execution Type vtype = *constant->result_type; vtype.is_constant = true;
    //
    // RIRTypeId real_vtype = this->analyser->rir_ctx->types->push(vtype);

    RIRTypeId real_vtype = type_id;
    RIRTypeId ptr_type = this->analyser->rir_ctx->types->push({
        .kind = RIRTypeKind::Pointer,
        .child = real_vtype,
        // TODO: Readd constant types
    });

    // Return Pointer to constant
    UIRLiteral lit_out = {.lit_type = ptr_type};
    lit_out.data.kind = UIRRawDataKind::Pointer;

    if (resolved_value->kind == UIRResolvedKind::Inst) {
      lit_out.data.ptr = {.kind = UIRPlaceKind::Inst,
                          .inst = resolved_value->inst};
    } else {
      lit_out.data.ptr = {.kind = UIRPlaceKind::Raw,
                          .data = &resolved_value->literal.data};
    }

    return lit_out;
  }

  assert(0 && "Failed to get value during compile-time");
}
