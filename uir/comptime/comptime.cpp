#include "comptime.hpp"
#include "../analysis/define.hpp"
#include "../literal.hpp"
#include "../types.hpp"
#include "../uir.hpp"
#include "containers.hpp"
#include "define.hpp"
#include "rir/constant.hpp"
#include "rir/type.hpp"
#include "uir/analysis/analysis.hpp"
#include <cassert>
#include <cinttypes>
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

void execute(UIRComptime *state, UIRModule *module, UIRValue *inst) {
  ComptimeStackFrame *frame = state->currentStack();

  switch (inst->kind) {
  case UIRValueKind::LocalVariable: {
    // Don't allocate another field
    size_t *ptr_idx = frame->lookup.get(inst);
    if (ptr_idx != nullptr) {
      return;
    }

    // Get Type
    UIRLiteral ty_lit =
        state->getValue(frame, module, inst->local_variable.type);
    // TODO: Confirm `ty_lit` is of `UIRLiteralKind::TypeId`

    // Allocate value
    UIRLiteral *value_lit = frame->add(nullptr);
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
    UIRLiteral *lit_out = frame->add(inst);
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
    UIRLiteral ptr = state->getValue(frame, module, inst->load.ptr);
    assert(ptr.data.kind == UIRRawDataKind::Pointer);

    // Create result
    UIRLiteral *lit_out = frame->add(inst);
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
    UIRLiteral ptr = state->getValue(frame, module, inst->store.ptr);
    UIRLiteral value = state->getValue(frame, module, inst->store.value);

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
    UIRLiteral ty_lit = state->getValue(frame, module, inst->load.ptr);
    // TODO: Confirm `ty_lit` is of `UIRLiteralKind::TypeId`
    // TODO: Compare types

    UIRLiteral *arg_value = frame->values.get(frame->arg_count);
    frame->arg_count += 1;

    UIRLiteral *lit_out = frame->add(inst);
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
  case UIRValueKind::Call: {
    state->pushStack();

    // Get Function
    UIRValue *function = nullptr;
    if (inst->call.callee->kind == UIRValueKind::Function) {
      function = inst->call.callee;
    } else {
      // FIXME:
      // UIRLiteral fn = state->getValue(frame, module, inst->call.callee);
      // assert(fn.kind == UIRLiteralKind::Instruction);
      // function = fn.instruction;
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

    *(frame->add(inst)) = result;
    return;
  }
  case UIRValueKind::Index: {
    // TODO: Implement compile-time Indexing
    break;
  }
  case UIRValueKind::BinOp: {
    *(frame->add(inst)) = executeBinary(state, module, frame, inst);
    return;
  }
  case UIRValueKind::Return: {
    UIRLiteral result;
    if (inst->ret.value.isSome()) {
      result = state->getValue(frame, module, inst->ret.value.get());
    } else {
      result.lit_type =
          state->analyser->rir_ctx->types->push({.kind = RIRTypeKind::Void});
      result.data.kind = UIRRawDataKind::Void;
    }

    *(frame->add(inst)) = result;
    return;
  }

  case UIRValueKind::Comptime: {
    state->pushStack();
    UIRBlock *entry = inst->comptime.blocks.get(0);
    executeProgram(state, module, entry);

    UIRLiteral result = **state->currentStack()->values.back();
    state->popStack();

    *(frame->add(inst)) = result;
    return;
  }
  case UIRValueKind::TypeOf: {
    UIRLiteral *lit_out = frame->add(inst);
    lit_out->data.kind = UIRRawDataKind::TypeId;

    UIRResolved *resolved_inst =
        state->analyser->resolved_mapping.get(inst->_typeof);
    if (resolved_inst == nullptr) {
      UIRLiteral comptime_result =
          state->getValue(frame, module, inst->_typeof);
      lit_out->data._typeid = comptime_result.lit_type.get();
      return;
    }

    switch (resolved_inst->kind) {
    case UIRResolvedKind::Inst: {
      RIRValue *rir_inst =
          state->analyser->rir_ctx->getInst(resolved_inst->inst);
      lit_out->data._typeid = rir_inst->result;
      break;
    }
    case UIRResolvedKind::Literal: {
      lit_out->data._typeid = resolved_inst->literal.lit_type.get();
      break;
    }
    }
    return;
  }

  case UIRValueKind::GlobalVariable: {
    *(frame->add(inst)) = state->getValue(nullptr, module, inst);
    return;
  }

  case UIRValueKind::Literal: {
    *(frame->add(inst)) = inst->literal;
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
      UIRValue *param = inst->function.parameter_types.ptr[i];

      UIRLiteral *param_literal = *executeGetReturn(state, module, param);
      assert(param_literal->data.kind == UIRRawDataKind::TypeId);
      raw_type.function.arguments.ptr[i] = param_literal->data._typeid;
    }

    // Analyse Return Type
    UIRLiteral *return_literal =
        *executeGetReturn(state, module, inst->function.return_type);
    raw_type.function._return = return_literal->data._typeid;

    // Get final type
    UIRLiteral *lit_out = frame->add(inst);
    lit_out->data.kind = UIRRawDataKind::TypeId;
    lit_out->data._typeid = state->analyser->rir_ctx->types->push(raw_type);
    return;
  }
  case UIRValueKind::Pointer: {
    UIRLiteral lit = state->getValue(frame, module, inst->pointer);

    UIRLiteral *lit_out = frame->add(inst);
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
    raw_type._struct.unique = (uint64_t)reinterpret_cast<uintptr_t>(inst);

    RIRTypeId struct_type_id = state->analyser->rir_ctx->types->push(raw_type);

    // Fields
    RIRType *struct_type = state->analyser->rir_ctx->getType(struct_type_id);
    struct_type->_struct.fields = {
        .ptr = (RIRTypeId *)state->arena->alloc(sizeof(RIRTypeId) *
                                                inst->_struct.fields.len),
        .len = inst->_struct.fields.len,
    };
    for (size_t i = 0; i < inst->_struct.fields.len; i++) {
      UIRStruct::Field *field = inst->_struct.fields.ptr + i;
      UIRLiteral type_lit = state->getValue(frame, module, field->type);
      assert(type_lit.data.kind == UIRRawDataKind::TypeId);
      struct_type->_struct.fields.ptr[i] = type_lit.data._typeid;
    }

    UIRLiteral *lit_out = frame->add(inst);
    lit_out->data = {.kind = UIRRawDataKind::TypeId, ._typeid = struct_type_id};
    return;
  }
  // FIXME:
  // case UIRValueKind::Enum: {
  //   RIRType raw_type = {
  //       .kind = RIRTypeKind::Enum,
  //       // TODO: Readd constant types
  //   };
  //   raw_type._enum.unique = (uint64_t)reinterpret_cast<uintptr_t>(inst);
  //
  //   RIRTypeId enum_type_id = state->analyser->rir_ctx->types->push(raw_type);
  //
  //   // Represent Type
  //   RIRType *enum_type = state->analyser->rir_ctx->getType(enum_type_id);
  //   UIRLiteral repr_lit = state->getValue(frame, module,
  //   inst->_enum.repr_type); enum_type->_enum.repr = repr_lit._typeid;
  //
  //   // Members
  //   int64_t next_value = 0;
  //   for (size_t i = 0; i < inst->_enum.members.len; i++) {
  //     UIREnum::Member *member = inst->_enum.members.ptr + i;
  //
  //     UIRLiteral result;
  //     if (member->constant != nullptr) {
  //       result = **executeGetReturn(state, module, member->constant);
  //     } else {
  //       result.kind = UIRLiteralKind::Typed;
  //       result._int = next_value;
  //     }
  //
  //     result.lit_type = repr_lit._typeid;
  //
  //     member->constant->literal = result;
  //     member->constant->kind = UIRValueKind::Literal;
  //     member->constant->result_type = member->constant->literal.lit_type;
  //     next_value = member->constant->literal._int + 1;
  //   }
  //
  //   return {
  //       .lit_type = state->ctx->type_cache->get({.kind = TypeKind::TypeId}),
  //       .kind = UIRLiteralKind::Typed,
  //       ._typeid = enum_type,
  //   };
  // }
  case UIRValueKind::Union: {
    RIRType raw_type = {
        .kind = RIRTypeKind::Union,
        // TODO: Readd constant types
    };
    raw_type._union.unique = (uint64_t)reinterpret_cast<uintptr_t>(inst);

    RIRTypeId union_type_id = state->analyser->rir_ctx->types->push(raw_type);

    // Represent Type
    RIRType *union_type = state->analyser->rir_ctx->getType(union_type_id);
    UIRLiteral repr_lit =
        state->getValue(frame, module, inst->_union.repr_type);
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
      UIRLiteral type_lit = state->getValue(frame, module, field->type);
      assert(type_lit.data.kind = UIRRawDataKind::TypeId);
      union_type->_union.variants.ptr[i] = type_lit.data._typeid;
    }

    UIRLiteral *lit_out = frame->add(inst);
    lit_out->data = {.kind = UIRRawDataKind::TypeId, ._typeid = union_type_id};
    return;
  }
  case UIRValueKind::Namespace: {
    UIRLiteral *lit_out = frame->add(inst);
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

    UIRLiteral element = state->getValue(frame, module, inst->slice.element);
    assert(element.data.kind == UIRRawDataKind::TypeId);
    raw_type.slice.child = element.data._typeid;

    if (inst->slice.is_pointer) {
      raw_type.slice.length = -1;
    } else if (inst->slice.length != nullptr) {
      UIRLiteral length = state->getValue(frame, module, inst->slice.length);

      assert(length.data.kind = UIRRawDataKind::Int);
      if (length.lit_type.isSome()) {
        RIRType *length_type =
            state->analyser->rir_ctx->getType(length.lit_type.get());
        assert(length_type->integer.is_untyped ||
               (length_type->integer.bits =
                    -1 && !length_type->integer.is_signed));
      }

      raw_type.slice.length = length.data._int;
    } else {
      raw_type.slice.length = 0;
    }

    UIRLiteral *lit_out = frame->add(inst);
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

UIRLiteral **executeGetReturn(UIRComptime *state, UIRModule *module,
                              UIRValue *inst) {
  execute(state, module, inst);
  return state->currentStack()->values.back();
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
      assert(cond.data.kind == UIRRawDataKind::Bool &&
             "Condition was not boolean");

      if (cond.data._bool) {
        program = &inst->condbr.then->instructions;
      } else {
        program = &inst->condbr._else->instructions;
      }
      continue;
    }

    // Execute instruction
    execute(state, module, inst);
  }
}

UIRLiteral UIRComptime::execute(UIRModule *module, UIRValue *inst) {
  this->pushStack();
  UIRLiteral result = **executeGetReturn(this, module, inst);
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
    UIRResolved *resolved_constant =
        this->analyser->resolved_mapping.get(constant);
    if (resolved_constant == nullptr) {
      analyseGlobal(this->analyser, module, from);
      resolved_constant = this->analyser->resolved_mapping.get(constant);
    }

    RIRTypeId type_id;
    if (resolved_constant->kind == UIRResolvedKind::Inst) {
      type_id =
          this->analyser->rir_ctx->getInst(resolved_constant->inst)->result;
    } else if (resolved_constant->kind == UIRResolvedKind::Literal) {
      type_id = resolved_constant->literal.lit_type.get();
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

    if (resolved_constant->kind == UIRResolvedKind::Inst) {
      lit_out.data.ptr = {.kind = UIRPlaceKind::Inst,
                          .inst = resolved_constant->inst};
    } else {
      lit_out.data.ptr = {.kind = UIRPlaceKind::Raw,
                          .data = &resolved_constant->literal.data};
    }
  }

  size_t idx = *frame->lookup.get(from);
  return *frame->values.getUnchecked(idx);
}
