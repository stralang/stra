#include "analysis.hpp"
#include "../literal.hpp"
#include "../uir.hpp"
#include "define.hpp"
#include "rir/constant.hpp"
#include "rir/rir.hpp"
#include "rir/type.hpp"

void analyse(UIRAnalyser *analyser, UIRModule *module, UIRValue *inst) {
  if (analyser->resolved_mapping.get(inst) != nullptr) {
    return; // Already Analysed
  }

  switch (inst->kind) {
  case UIRValueKind::LocalVariable: {
    UIRLiteral type_literal =
        analyser->comptime_state.execute(module, inst->local_variable.type);
    expect(type_literal.data.kind == UIRRawDataKind::TypeId,
           inst->local_variable.type->source_location,
           "Field type must be a typeid");

    RIRTypeId type = analyser->rir_ctx->types->push({
        .kind = RIRTypeKind::Pointer,
        .child = type_literal.data._typeid,
    });

    // Create Instruction
    RIRValueId out_id =
        analyser->builder.buildLocalVariable(type_literal.data._typeid);
    analyser->resolved_mapping.insert(
        inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::Load: {
    analyse(analyser, module, inst->load.ptr);
    UIRResolved *ptr_id =
        analyser->resolved_mapping.get(inst->load.ptr); // FIXME: Confirm kind
    RIRValue *ptr_inst = analyser->rir_ctx->getInst(ptr_id->inst);
    RIRType *ptr_type = analyser->rir_ctx->getType(ptr_inst->result);
    expect(ptr_type->kind == RIRTypeKind::Pointer,
           inst->load.ptr->source_location, "!UIR! Can only load from pointer");

    // Create Instruction
    RIRValueId out_id =
        analyser->builder.buildLoad(ptr_id->inst, ptr_type->child);
    analyser->resolved_mapping.insert(
        inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::Store: {
    analyse(analyser, module, inst->store.ptr);
    analyse(analyser, module, inst->store.value);
    UIRResolved *ptr_id =
        analyser->resolved_mapping.get(inst->store.ptr); // FIXME: Confirm kind
    UIRResolved *value_id = analyser->resolved_mapping.get(
        inst->store.value); // FIXME: Confirm kind

    RIRValue *ptr_inst = analyser->rir_ctx->getInst(ptr_id->inst);
    RIRValue *value_inst = analyser->rir_ctx->getInst(value_id->inst);

    RIRType *ptr_type = analyser->rir_ctx->getType(ptr_inst->result);
    // TODO: autoCast(analyser, value_inst, ptr_type->child);
    expect(ptr_type->child == value_inst->result, inst->source_location,
           "Cannot assign non-matching types");

    // Create Instruction
    RIRValueId out_id =
        analyser->builder.buildStore(ptr_id->inst, value_id->inst);
    analyser->resolved_mapping.insert(
        inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::Arg: {
    UIRLiteral arg_literal =
        analyser->comptime_state.execute(module, inst->arg.type);
    expect(arg_literal.data.kind == UIRRawDataKind::TypeId,
           inst->arg.type->source_location, "Argument type must be typeid");

    // Create Instruction
    RIRValueId out_id = analyser->builder.buildArg(arg_literal.data._typeid);
    analyser->resolved_mapping.insert(
        inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::BinOp: {
    analyseBinary(analyser, module, inst);
    break;
  }
  case UIRValueKind::UnaryOp: {
    analyseUnary(analyser, module, inst);
    break;
  }
  case UIRValueKind::Call: {
    analyse(analyser, module, inst->call.callee);

    UIRResolved *callee_id = analyser->resolved_mapping.get(
        inst->call.callee); // FIXME: Confirm kind
    RIRValue *callee = analyser->rir_ctx->getInst(callee_id->inst);

    // Auto dereference
    RIRType *callee_type = analyser->rir_ctx->getType(callee->result);
    if (callee_type->kind == RIRTypeKind::Pointer) {
      callee_type = analyser->rir_ctx->getType(callee_type->child);
    }

    // Get function
    expect(callee_type->kind == RIRTypeKind::Function,
           inst->call.callee->source_location,
           "Callee must be a function. Got " << callee_type << "`");

    RIRType *fn_type = callee_type;

    // Arguments
    Slice<RIRValueId> arguments = Slice<RIRValueId>{
        .ptr = (RIRValueId *)analyser->allocator->alloc(
            sizeof(RIRValueId) * inst->call.arguments.len),
        .len = inst->call.arguments.len,
    };

    // Get receiver
    size_t initial_idx = 0;
    if (inst->call.receiver.isSome()) {
      UIRValue *uir_receiver_inst = inst->call.receiver.get();
      analyse(analyser, module, uir_receiver_inst);

      UIRResolved *receiver_id = analyser->resolved_mapping.get(
          uir_receiver_inst); // FIXME: Confirm kind

      bool receiver_is_valid =
          receiver_id->kind != UIRResolvedKind::Literal ||
          receiver_id->literal.data.kind != UIRRawDataKind::TypeId;

      if (receiver_is_valid) {
        expect(fn_type->function.arguments.len >= 1,
               uir_receiver_inst->source_location,
               "Receiver expects method with atleast 1 argument");

        RIRType *expected_type =
            analyser->rir_ctx->getType(fn_type->function.arguments.ptr[0]);
        // TODO:
        /*expect(compareTypes(expected_type, receiver_type),
               receiver_inst->source_location,
               "Receiver `" << receiver_type << "` doesn't match `"
                            << expected_type << "`");*/

        arguments[initial_idx] = receiver_id->inst;
        initial_idx = 1;
      }
    }

    // Analyse arguments
    for (size_t i = 0; i < inst->call.arguments.len; i++) {
      UIRValue *uir_arg = inst->call.arguments.ptr[i];
      if (i > fn_type->function.arguments.len - initial_idx) {
        expect(false, uir_arg->source_location, "Too many arguments");
        break;
      }

      RIRType *expected_type = analyser->rir_ctx->getType(
          fn_type->function.arguments.ptr[i + initial_idx]);
      analyse(analyser, module, uir_arg);
      UIRResolved *arg_id =
          analyser->resolved_mapping.get(uir_arg); // FIXME: Confirm kind
      RIRValue *arg = analyser->rir_ctx->getInst(arg_id->inst);
      // TODO: autoCast(analyser, arg, expected_type);
      // expect(compareTypes(expected_type, arg->result),
      // uir_arg->source_location,
      //        "Argument `" << arg->result << "` doesn't match expected `"
      //                     << expected_type << "`");

      arguments[i + initial_idx] = arg_id->inst;
    }
    // Create Instruction
    RIRValueId out_id = analyser->builder.buildCall(callee_id->inst, arguments,
                                                    fn_type->function._return);
    analyser->resolved_mapping.insert(
        inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::Index: {
    // Analyse Pointer
    analyse(analyser, module, inst->index.ptr);
    UIRResolved *ptr_id =
        analyser->resolved_mapping.get(inst->index.ptr); // FIXME: Confirm kind
    RIRValue *ptr = analyser->rir_ctx->getInst(ptr_id->inst);

    RIRType *ptr_type = analyser->rir_ctx->getType(ptr->result);
    RIRType *ptr_child_type = analyser->rir_ctx->getType(ptr_type->child);
    expect(ptr_child_type->kind == RIRTypeKind::Slice, inst->source_location,
           "Cannot index into non-slice");

    // Analyse Index
    // TODO: RIRTypeId usize_ty = analyser->rir_ctx->types->push({
    //     .kind = RIRTypeKind::Integer,
    //     .integer = {.is_untyped = false, .is_signed = false, .bits = -1},
    // });

    analyse(analyser, module, inst->index.index);
    UIRResolved *index_id = analyser->resolved_mapping.get(
        inst->index.index); // FIXME: Confirm kind
    RIRValue *index = analyser->rir_ctx->getInst(index_id->inst);
    // TODO: fixUntyped(analyser, index, usize_ty);
    RIRType *index_type = analyser->rir_ctx->getType(index->result);
    expect(index_type->kind == RIRTypeKind::Integer &&
               !index_type->integer.is_signed && index_type->integer.bits == -1,
           inst->index.index->source_location, "Index must be of type `usize`");

    // Create Instruction
    RIRTypeId result = analyser->rir_ctx->types->push(
        {.kind = RIRTypeKind::Pointer, .child = ptr_child_type->slice.child});
    RIRValueId out_id =
        analyser->builder.buildGEP(ptr_id->inst, index_id->inst, result);
    analyser->resolved_mapping.insert(
        inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::Range: {
    // TODO: Range analysis
    // analyse(analyser, module, inst->range.ptr);
    // analyse(analyser, module, inst->range.start);
    // analyse(analyser, module, inst->range.end);
    //
    // // Check Pointer
    // UIRValue *ptr = inst->range.ptr;
    // expect(ptr->result_type->child->kind == TypeKind::Slice ||
    //            ptr->result_type->child->kind == TypeKind::Pointer,
    //        inst->source_location, "Cannot range into non-slice/pointer");
    //
    // // Check Index and Length
    // Type *usize_ty = analyser->ctx->type_cache->get({
    //     .kind = TypeKind::Integer,
    //     .integer = {false, false, -1},
    // });
    // fixUntyped(analyser, inst->range.start, usize_ty);
    // fixUntyped(analyser, inst->range.end, usize_ty);
    //
    // UIRValue *start = inst->range.start;
    // RIRType *range_type = analyser->rir_ctx->getType(start->result_type);
    // expect(range_type->kind == RIRTypeKind::Integer &&
    //            !range_type->integer.is_untyped &&
    //            !range_type->integer.is_signed &&
    //            range_type->integer.bits == -1 &&
    //            start->result_type == inst->range.end->result_type,
    //        start->source_location, "Range start and end must both be
    //        `usize`");
    //
    // // Get Result type
    // Type ty = {.kind = TypeKind::Slice};
    // ty.slice.length = 0;
    // if (ptr->result_type->child->kind == TypeKind::Pointer) {
    //   ty.slice.type = ptr->result_type->child->child;
    // } else {
    //   ty.slice.type = ptr->result_type->child->slice.type;
    // }
    //
    // inst->result_type = analyser->ctx->type_cache->get(ty);
    break;
  }
  case UIRValueKind::LookupPtr: {
    analyseLookupPtr(analyser, module, inst);
    break;
  }
  case UIRValueKind::LookupValue: {
    analyseLookupValue(analyser, module, inst);
    break;
  }
  case UIRValueKind::Aggregate: {
    analyseAggregate(analyser, module, inst);
    break;
  }
  case UIRValueKind::Return: {
    expect(inst->parent->parent->kind == UIRValueKind::Function,
           inst->source_location,
           "A runtime return must be a child of a function");
    UIRResolved *function_id = analyser->resolved_mapping.get(
        inst->parent->parent); // FIXME: Confirm kind
    RIRValue *function = analyser->rir_ctx->getInst(function_id->inst);
    RIRType *fn_type = analyser->rir_ctx->getType(function->result);
    RIRType *expected_type =
        analyser->rir_ctx->getType(fn_type->function._return);

    Option<RIRValueId> ret_value = {};
    if (inst->ret.value.isNone()) {
      expect(expected_type->kind != RIRTypeKind::Void, inst->source_location,
             "Function expects return value");
    } else {
      UIRValue *uir_value_inst = inst->ret.value.get();
      analyse(analyser, module, uir_value_inst);
      UIRResolved *value_id = analyser->resolved_mapping.get(uir_value_inst);

      // TODO: autoCast(analyser, value_inst, expected_type);
      // expect(compareTypes(expected_type, value_inst->result_type),
      //        value_inst->source_location,
      //        "Unexpected return type. Got `"
      //            << value_inst->result_type << "` Expected `" <<
      //            expected_type
      //            << "`");

      ret_value.setSome(value_id->inst);
    }

    // Create Instruction
    RIRValueId out_id = analyser->builder.buildReturn(ret_value);
    analyser->resolved_mapping.insert(
        inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::Branch: {
    RIRBlockId *dest =
        analyser->resolved_block_mapping.get(inst->br); // FIXME: Confirm exists
    RIRValueId out_id = analyser->builder.buildBranch(*dest);
    analyser->resolved_mapping.insert(
        inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::CondBranch: {
    analyse(analyser, module, inst->condbr.condition);
    UIRResolved *condition_id =
        analyser->resolved_mapping.get(inst->condbr.condition);
    RIRValue *condition = analyser->rir_ctx->getInst(condition_id->inst);
    RIRType *condition_type = analyser->rir_ctx->getType(condition->result);
    expect(condition_type->kind == RIRTypeKind::Bool, inst->source_location,
           "Conditional must be Bool");

    // Get Blocks
    RIRBlockId *then = analyser->resolved_block_mapping.get(
        inst->condbr.then); // FIXME: Confirm exists
    RIRBlockId *_else = analyser->resolved_block_mapping.get(
        inst->condbr._else); // FIXME: Confirm exists

    // Create Instruction
    RIRValueId out_id =
        analyser->builder.buildCondBranch(condition_id->inst, *then, *_else);
    analyser->resolved_mapping.insert(
        inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::Switch: {
    analyse(analyser, module, inst->_switch.condition);
    UIRResolved *condition_id = analyser->resolved_mapping.get(
        inst->_switch.condition); // FIXME: Confirm kind

    RIRBlockId *default_dest = analyser->resolved_block_mapping.get(
        inst->_switch.default_block); // FIXME: Confirm exists

    // Create Switch Instruction
    RIRValueId out_id = analyser->builder.buildSwitch(
        condition_id->inst, *default_dest, inst->_switch.onvals.len);
    analyser->resolved_mapping.insert(
        inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});

    // Create Cases
    for (size_t i = 0; i < inst->_switch.onvals.len; i++) {
      // Get Constant
      UIRValue *on_val = inst->_switch.onvals.ptr[i];
      analyse(analyser, module, on_val);

      UIRLiteral onval_literal =
          analyser->comptime_state.execute(module, on_val);
      RIRValueId constant_id = analyser->builder.buildConstant(
          onval_literal.lit_type.get(), {.kind = RIRConstantKind::Integer,
                                         .integer = onval_literal.data._int});

      // Add Case
      RIRBlockId *dest = analyser->resolved_block_mapping.get(
          inst->_switch.blocks.ptr[i]); // FIXME: Confirm exists

      analyser->builder.addCase(out_id, constant_id, *dest, i);
    }
    break;
  }

  case UIRValueKind::Comptime: {
    UIRLiteral literal = analyser->comptime_state.execute(module, inst);

    Option<RIRTypeId> type_id = literal.lit_type;
    RIRConstant constant;
    switch (literal.data.kind) {
    case UIRRawDataKind::Bool: {
      constant.kind = RIRConstantKind::Bool;
      constant._bool = literal.data._bool;

      if (type_id.isNone()) {
        type_id = analyser->rir_ctx->types->push({.kind = RIRTypeKind::Bool});
      }
      break;
    }
    case UIRRawDataKind::Int: {
      constant.kind = RIRConstantKind::Integer;
      constant.integer = literal.data._int;

      if (type_id.isNone()) {
        type_id = analyser->rir_ctx->types->push({
            .kind = RIRTypeKind::Integer,
            .integer = {.is_signed = true, .bits = 32},
        });
      }
      break;
    }
    case UIRRawDataKind::Float: {
      constant.kind = RIRConstantKind::Float;
      constant._float = literal.data._float;

      if (type_id.isNone()) {
        type_id = analyser->rir_ctx->types->push(
            {.kind = RIRTypeKind::Float, ._float = {.bits = 32}});
      }
      break;
    }
    case UIRRawDataKind::TypeId:
    case UIRRawDataKind::Namespace: {
      analyser->resolved_mapping.insert(
          inst, {.kind = UIRResolvedKind::Literal, .literal = literal});
      return;
    }
    }

    // Create Instruction
    RIRValueId out_id =
        analyser->builder.buildConstant(type_id.get(), constant);
    analyser->resolved_mapping.insert(
        inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::TypeOf: {
    UIRLiteral literal = analyser->comptime_state.execute(module, inst);
    analyser->resolved_mapping.insert(
        inst, {.kind = UIRResolvedKind::Literal, .literal = literal});
    break;
  }

  case UIRValueKind::GlobalVariable:
  case UIRValueKind::Function: {
    analyseGlobal(analyser, module, inst);
    break;
  }
  case UIRValueKind::Literal: {
    UIRLiteral literal = inst->literal;

    Option<RIRTypeId> type_id = literal.lit_type;
    RIRConstant constant;
    switch (literal.data.kind) {
    case UIRRawDataKind::Bool: {
      constant.kind = RIRConstantKind::Bool;
      constant._bool = literal.data._bool;

      if (type_id.isNone()) {
        type_id = analyser->rir_ctx->types->push({.kind = RIRTypeKind::Bool});
      }
      break;
    }
    case UIRRawDataKind::Int: {
      constant.kind = RIRConstantKind::Integer;
      constant.integer = literal.data._int;

      if (type_id.isNone()) {
        type_id = analyser->rir_ctx->types->push({
            .kind = RIRTypeKind::Integer,
            .integer = {.is_signed = true, .bits = 32},
        });
      }
      break;
    }
    case UIRRawDataKind::Float: {
      constant.kind = RIRConstantKind::Float;
      constant._float = literal.data._float;

      if (type_id.isNone()) {
        type_id = analyser->rir_ctx->types->push(
            {.kind = RIRTypeKind::Float, ._float = {.bits = 32}});
      }
      break;
    }
    }

    // Create Instruction
    RIRValueId out_id =
        analyser->builder.buildConstant(type_id.get(), constant);
    analyser->resolved_mapping.insert(
        inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  default: {
    std::cerr << "TODO: Implement analysis of `" << std::hex
              << (uint16_t)inst->kind << "`\n";
    std::abort();
    break;
  }
  }
}

void analyseBlock(UIRAnalyser *analyser, UIRModule *module, UIRBlock *block) {
  for (size_t i = 0; i < block->instructions.length; i++) {
    analyse(analyser, module, block->instructions.getUnchecked(i));
  }
}

void analyseScope(UIRAnalyser *analyser, UIRModule *module, UIRScope *scope) {
  for (size_t i = 0; i < scope->list.length; i++) {
    analyse(analyser, module, scope->list.getUnchecked(i));
  }
}

RIRContext *UIRAnalyser::analyse() {
  for (size_t i = 0; i < this->ctx->modules.len(); i++) {
    UIRModule *uir_mod = this->ctx->modules.getPtrUnchecked(i);
    RIRModule *rir_mod = this->rir_ctx->modules.getPtrUnchecked(i);
    this->builder.module = rir_mod;

    analyseScope(this, uir_mod, uir_mod->definitions);
  }
  return this->rir_ctx;
}

void UIRAnalyser::init(Allocator *allocator) {
  this->allocator = allocator;
  this->arena.init(allocator, 1024 * 1024 * 8);

  this->comptime_state.init(allocator, &this->arena);
  this->comptime_state.ctx = this->ctx;
  this->comptime_state.analyser = this;

  // RIR
  this->resolved_mapping.init(allocator, 4096);
  this->resolved_block_mapping.init(allocator, 512);

  this->rir_ctx = (RIRContext *)this->allocator->alloc(sizeof(RIRContext));
  this->rir_ctx->init(allocator, allocator);
  this->rir_ctx->types = this->ctx->types;
  this->builder.ctx = this->rir_ctx;

  for (size_t i = 0; i < this->ctx->modules.len(); i++) {
    RIRModule mod = {.id = (uint32_t)i};
    mod.init(this->rir_ctx->allocator, this->rir_ctx->allocator);
    this->rir_ctx->modules.push(mod);
  }
}

void UIRAnalyser::deinit() {
  this->arena.deinit();
  this->resolved_mapping.deinit();
  this->resolved_block_mapping.deinit();
}
