#include "comptime.hpp"
#include "../analysis/define.hpp"
#include "../literal.hpp"
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

    if (inst->local_variable.initial.isSome()) {
      UIRLiteral lit =
          state->getValue(frame, inst->local_variable.initial.get());
      value_lit->data = lit.data;
      // FIXME: Check initial type against type
    } else {
      RIRType *real_type =
          state->analyser->rir_ctx->getType(ty_lit.data._typeid);
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
    }

    // Create pointer
    UIRLiteral *lit_out = frame->add(inst->id);
    lit_out->lit_type = state->analyser->rir_ctx->types->push({
        .kind = RIRTypeKind::Pointer,
        .pointer = {ty_lit.data._typeid, inst->local_variable.is_const},
    });
    lit_out->data.kind = UIRRawDataKind::Pointer;
    lit_out->data.ptr = {.kind = UIRPlaceKind::Raw, .data = &value_lit->data};
    return;
  }
  case UIRValueKind::Load: {
    UIRLiteral ptr = state->getValue(frame, inst->load.ptr);
    ctexpect(ptr.data.kind == UIRRawDataKind::Pointer, inst->source_location,
             "Pointer must be of pointer type");

    // Create result
    UIRLiteral *lit_out = frame->add(inst->id);
    if (ptr.lit_type.isSome()) {
      RIRType *real_type =
          state->analyser->rir_ctx->getType(ptr.lit_type.get());
      lit_out->lit_type = real_type->pointer.child;
    }

    if (ptr.data.ptr.kind == UIRPlaceKind::Inst) {
      RIRValue *rir_inst = state->analyser->rir_ctx->getInst(ptr.data.ptr.inst);
      ctexpect(rir_inst->global_variable.constant.isSome(),
               inst->source_location,
               "!UIR Internal! global variable initial is not a constant");
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

    if (ptr.lit_type.isSome()) {
      RIRType *ptr_type = state->analyser->rir_ctx->getType(ptr.lit_type.get());
      ctexpect(!ptr_type->pointer.const_pointee, inst->source_location,
               "Cannot assign to constant");
    }

    // TODO: Compare types

    ctexpect(ptr.data.ptr.kind == UIRPlaceKind::Raw, inst->source_location,
             "Pointer must be compile-time known");
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
        .pointer = {ty_lit.data._typeid, inst->arg.is_const},
    });
    lit_out->data.kind = UIRRawDataKind::Pointer;
    lit_out->data.ptr = {
        .kind = UIRPlaceKind::Raw,
        .data = &arg_value->data,
    };
    return;
  }
  case UIRValueKind::RetType: {
    // TODO: Enforce return type for `ret` instructions
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
      UIRLiteral fn = state->getValue(frame, inst->call.callee);
      ctexpect(fn.data.kind == UIRRawDataKind::Function, inst->source_location,
               "Callee must be a function");
      function = state->ctx->getInst(fn.data.function);
    }

    // Inject Arguments
    for (size_t i = 0; i < inst->call.arguments.len; i++) {
      UIRValueId arg = inst->call.arguments.ptr[i];
      UIRLiteral value = state->getValue(frame, arg);
      state->currentStack()->inject(value);
    }

    if (!function->function.undefined) {
      executeProgram(state, function->function.blocks.get(0));
    } else {
      executeBuiltinCall(state, inst, function);
    }

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
    state->pushStack(frame);
    executeProgram(state, inst->comptime.blocks.get(0));

    UIRLiteral result = **state->currentStack()->values.back();
    state->popStack();

    *(frame->add(inst->id)) = result;
    return;
  }
  case UIRValueKind::TypeOf: {
    UIRLiteral *lit_out = frame->add(inst->id);
    lit_out->data.kind = UIRRawDataKind::TypeId;

    UIRLiteral infer_from;
    UIRResolved *resolved_inst =
        state->analyser->resolved_mapping.get(inst->_typeof);
    if (resolved_inst == nullptr) {
      UIRLiteral comptime_result = state->getValue(frame, inst->_typeof);
      infer_from = comptime_result;
    } else if (resolved_inst->kind == UIRResolvedKind::Inst) {
      // Get type from instruction
      RIRValue *rir_inst =
          state->analyser->rir_ctx->getInst(resolved_inst->inst);
      lit_out->data._typeid = rir_inst->result;
      return;
    } else {
      // Get type from literal
      assert(resolved_inst->kind == UIRResolvedKind::Literal);
      infer_from = resolved_inst->literal;
    }

    // Get or guess type
    if (infer_from.lit_type.isSome()) {
      lit_out->data._typeid = infer_from.lit_type.get();
    } else {
      // Guess type from data
      switch (infer_from.data.kind) {
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
      case UIRRawDataKind::Pointer:
      case UIRRawDataKind::Slice:
      case UIRRawDataKind::Function:
      case UIRRawDataKind::Namespace: {
        ctexpect(false, inst->source_location, "Cannot guess type");
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
    assert(0 && "Cannot execute function instruction in comptime");
    break;
  }
  case UIRValueKind::Pointer: {
    UIRLiteral lit = state->getValue(frame, inst->pointer.pointee);

    UIRLiteral *lit_out = frame->add(inst->id);
    lit_out->data.kind = UIRRawDataKind::TypeId;
    lit_out->data._typeid = state->analyser->rir_ctx->types->push({
        .kind = RIRTypeKind::Pointer,
        .pointer = {lit.data._typeid, inst->pointer.const_pointee},
    });
    return;
  }
  case UIRValueKind::Struct: {
    RIRType raw_type = {
        .kind = RIRTypeKind::Struct,
        // TODO: Readd constant types
    };
    RIRTypeId struct_type_id = state->analyser->rir_ctx->types->push(raw_type);
    computeStructType(state, inst, struct_type_id);
    return;
  }
  case UIRValueKind::Enum: {
    RIRTypeId enum_type_id =
        state->analyser->rir_ctx->types->push({.kind = RIRTypeKind::Enum});
    computeEnumType(state, inst, enum_type_id);
    return;
  }
  case UIRValueKind::Union: {
    RIRType raw_type = {
        .kind = RIRTypeKind::Union,
        // TODO: Readd constant types
    };
    RIRTypeId union_type_id = state->analyser->rir_ctx->types->push(raw_type);
    computeUnionType(state, inst, union_type_id);
    return;
  }
  case UIRValueKind::Namespace: {
    UIRLiteral *lit_out = frame->add(inst->id);
    lit_out->data = {.kind = UIRRawDataKind::Namespace,
                     ._namespace = inst->_namespace.definitions};
    return;
  }
  case UIRValueKind::Slice: {
    RIRType raw_type = {.kind =
                            RIRTypeKind::Slice}; // TODO: Readd constant types

    UIRLiteral element = state->getValue(frame, inst->slice.element);
    ctexpect(element.data.kind == UIRRawDataKind::TypeId, inst->source_location,
             "Slice element type must be of TypeId");
    raw_type.slice.child = element.data._typeid;

    if (inst->slice.is_pointer) {
      raw_type.slice.length = -1;
    } else if (inst->slice.length.isSome()) {
      UIRLiteral length = state->getValue(frame, inst->slice.length.get());

      ctexpect(length.data.kind == UIRRawDataKind::Int, inst->source_location,
               "Slice length must be of `usize`");
      if (length.lit_type.isSome()) {
        RIRType *length_type =
            state->analyser->rir_ctx->getType(length.lit_type.get());
        ctexpect(length_type->integer.bits == -1 &&
                     !length_type->integer.is_signed,
                 inst->source_location, "Slice length must be of `usize`");
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
      ctexpect(cond.data.kind == UIRRawDataKind::Bool, inst->source_location,
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

void UIRComptime::pushStack(Option<ComptimeStackFrame *> parent) {
  ComptimeStackFrame frame;
  frame.lookup.init(this->allocator, 32);
  frame.values.init(this->allocator, 32);
  frame.arena.init(this->allocator, 1024 * 1024);
  frame.parent = parent;
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
  } else if (frame->parent.isSome()) {
    return this->getValue(frame->parent.get(), from);
  }

  // Get Instruction
  UIRValue *from_inst = this->ctx->getInst(from);
  if (from_inst->kind == UIRValueKind::Function) {
    return {.data = {.kind = UIRRawDataKind::Function, .function = from}};
  } else if (from_inst->kind == UIRValueKind::Namespace) {
    return {.data = {.kind = UIRRawDataKind::Namespace,
                     ._namespace = from_inst->_namespace.definitions}};
  }

  // Get Resolved data
  UIRResolved *resolved_value = this->analyser->resolved_mapping.get(from);
  if (resolved_value == nullptr) {
    analyse(this->analyser, from_inst);
    resolved_value = this->analyser->resolved_mapping.get(from);
  }

  // Cycle and Posion assertion
  // FIXME: This needs to be handled better
  assert(resolved_value->kind != UIRResolvedKind::Pending &&
         resolved_value->kind != UIRResolvedKind::Poison);

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

    RIRTypeId real_vtype = type_id;
    RIRTypeId ptr_type = this->analyser->rir_ctx->types->push({
        .kind = RIRTypeKind::Pointer,
        .pointer = {real_vtype, true},
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

  ctexpect(false, from_inst->source_location,
           "Failed to get value during compile-time");
}
