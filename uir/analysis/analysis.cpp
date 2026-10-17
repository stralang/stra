#include "analysis.hpp"
#include "../literal.hpp"
#include "../uir.hpp"
#include "allocator.hpp"
#include "define.hpp"
#include "rir/constant.hpp"
#include "rir/debug.hpp"
#include "rir/rir.hpp"
#include "rir/type.hpp"
#include "uir/comptime/define.hpp"
#include <cassert>

RIRValueId getInstFromResolved(UIRAnalyser *analyser, UIRResolved *resolved,
                               Option<RIRTypeId> default_type) {
  if (resolved->kind == UIRResolvedKind::Inst) {
    return resolved->inst;
  }

  assert(resolved->kind == UIRResolvedKind::Literal);
  RIRConstant rir_const =
      uirRawDataToRIRConstant(analyser->allocator, resolved->literal.data);
  RIRTypeId out_type;
  if (resolved->literal.lit_type.isSome()) {
    out_type = resolved->literal.lit_type.get();
  } else if (default_type.isSome()) {
    out_type = default_type.get();
  } else {
    // Guess Type
    switch (resolved->literal.data.kind) {
    case UIRRawDataKind::Void: {
      out_type = analyser->rir_ctx->types->push({.kind = RIRTypeKind::Void});
      break;
    }
    case UIRRawDataKind::Bool: {
      out_type = analyser->rir_ctx->types->push({.kind = RIRTypeKind::Bool});
      break;
    }
    case UIRRawDataKind::Int: {
      out_type = analyser->rir_ctx->types->push(
          {.kind = RIRTypeKind::Integer, .integer = {true, 32}});
      break;
    }
    case UIRRawDataKind::Float: {
      out_type = analyser->rir_ctx->types->push(
          {.kind = RIRTypeKind::Float, .float_bits = 32});
      break;
    }
    case UIRRawDataKind::Pointer:
    case UIRRawDataKind::Slice:
    case UIRRawDataKind::Function:
    case UIRRawDataKind::Namespace: {
      assert(0 && "Cannot guess type");
      break;
    }
    case UIRRawDataKind::TypeId: {
      out_type = analyser->rir_ctx->types->push({.kind = RIRTypeKind::TypeId});
      break;
    }
    }
  }

  return analyser->builder.buildConstant(out_type, rir_const);
}

void analyse(UIRAnalyser *analyser, UIRValue *inst) {
  UIRResolved *_previous_analysis = analyser->resolved_mapping.get(inst->id);
  if (_previous_analysis != nullptr) {
    expect(_previous_analysis->kind != UIRResolvedKind::Pending,
           inst->source_location, "Cyclic dependency detected");
    return; // Already Analysed
  }

  analyser->resolved_mapping.insert(inst->id,
                                    {.kind = UIRResolvedKind::Pending});

  RIRModule *prev_mod = analyser->builder.module;
  analyser->builder.module = analyser->rir_ctx->modules.getPtr(inst->id.module);

  switch (inst->kind) {
  case UIRValueKind::LocalVariable: {
    UIRValue *type_inst = analyser->ctx->getInst(inst->local_variable.type);
    UIRLiteral type_literal = analyser->comptime_state.execute(type_inst);
    expect(type_literal.data.kind == UIRRawDataKind::TypeId,
           type_inst->source_location, "Field type must be a typeid");

    RIRTypeId type = analyser->rir_ctx->types->push({
        .kind = RIRTypeKind::Pointer,
        .child = type_literal.data._typeid,
    });

    // Create Instruction
    RIRValueId out_id =
        analyser->builder.buildLocalVariable(type_literal.data._typeid);
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::Load: {
    UIRValue *ptr_inst = analyser->ctx->getInst(inst->load.ptr);
    analyse(analyser, ptr_inst);

    UIRResolved *ptr_resolved = analyser->resolved_mapping.get(inst->load.ptr);
    checkResolved(ptr_resolved);
    if (ptr_resolved->kind == UIRResolvedKind::Literal) {
      // Fold comptime pointer
      UIRLiteral out = analyser->comptime_state.execute(inst);
      analyser->resolved_mapping.insert(
          inst->id, {.kind = UIRResolvedKind::Literal, .literal = out});
      break;
    }

    // Generate runtime Load
    assert(ptr_resolved->kind == UIRResolvedKind::Inst);

    RIRValue *resolved_ptr_inst =
        analyser->rir_ctx->getInst(ptr_resolved->inst);
    RIRType *resolved_ptr_type =
        analyser->rir_ctx->getType(resolved_ptr_inst->result);
    expect(resolved_ptr_type->kind == RIRTypeKind::Pointer,
           ptr_inst->source_location, "!UIR! Can only load from pointer");

    // Create Instruction
    RIRValueId out_id = analyser->builder.buildLoad(ptr_resolved->inst,
                                                    resolved_ptr_type->child);
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::Store: {
    UIRValue *ptr_inst = analyser->ctx->getInst(inst->store.ptr);
    UIRValue *value_inst = analyser->ctx->getInst(inst->store.value);

    analyse(analyser, ptr_inst);
    analyse(analyser, value_inst);

    UIRResolved *ptr_resolved = analyser->resolved_mapping.get(ptr_inst->id);
    checkResolved(ptr_resolved);

    // Get ptr
    RIRValue *resolved_ptr_inst;
    if (ptr_resolved->kind == UIRResolvedKind::Inst) {
      resolved_ptr_inst = analyser->rir_ctx->getInst(ptr_resolved->inst);
    } else if (ptr_resolved->kind == UIRResolvedKind::Literal &&
               ptr_resolved->literal.data.kind == UIRRawDataKind::Pointer &&
               ptr_resolved->literal.data.ptr.kind == UIRPlaceKind::Inst) {
      resolved_ptr_inst =
          analyser->rir_ctx->getInst(ptr_resolved->literal.data.ptr.inst);
    } else {
      expect(false, ptr_inst->source_location,
             "Cannot store to comptime pointer");
    }

    RIRType *resolved_ptr_type =
        analyser->rir_ctx->getType(resolved_ptr_inst->result);

    // Get value
    UIRResolved *resolved_value =
        analyser->resolved_mapping.get(inst->store.value);
    checkResolved(resolved_value);

    RIRValueId value_resolved =
        getInstFromResolved(analyser, resolved_value, resolved_ptr_type->child);
    RIRValue *resolved_value_inst = analyser->rir_ctx->getInst(value_resolved);

    // Check types
    resolved_value_inst =
        autoCast(analyser, resolved_value_inst, resolved_ptr_type->child);
    expect(resolved_ptr_type->child == resolved_value_inst->result,
           inst->source_location, "Cannot assign non-matching types");

    // Create Instruction
    RIRValueId out_id = analyser->builder.buildStore(resolved_ptr_inst->id,
                                                     resolved_value_inst->id);
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::Arg: {
    UIRValue *type_inst = analyser->ctx->getInst(inst->arg.type);
    UIRLiteral arg_literal = analyser->comptime_state.execute(type_inst);
    expect(arg_literal.data.kind == UIRRawDataKind::TypeId,
           type_inst->source_location, "Argument type must be typeid");

    // Create Instruction
    RIRValueId out_id = analyser->builder.buildArg(arg_literal.data._typeid);
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::BinOp: {
    analyseBinary(analyser, inst);
    break;
  }
  case UIRValueKind::UnaryOp: {
    analyseUnary(analyser, inst);
    break;
  }
  case UIRValueKind::Call: {
    analyseCall(analyser, inst);
    break;
  }
  case UIRValueKind::Index: {
    // Analyse Pointer
    analyse(analyser, analyser->ctx->getInst(inst->index.ptr));

    UIRResolved *ptr_resolved =
        analyser->resolved_mapping.get(inst->index.ptr); // FIXME: Confirm kind
    checkResolved(ptr_resolved);
    RIRValue *resolved_ptr_inst =
        analyser->rir_ctx->getInst(ptr_resolved->inst);

    RIRType *resolved_ptr_type =
        analyser->rir_ctx->getType(resolved_ptr_inst->result);
    RIRType *resolved_ptr_child_type =
        analyser->rir_ctx->getType(resolved_ptr_type->child);
    expect(resolved_ptr_child_type->kind == RIRTypeKind::Slice,
           inst->source_location, "Cannot index into non-slice");

    // Analyse Index
    UIRValue *index_inst = analyser->ctx->getInst(inst->index.index);
    analyse(analyser, index_inst);
    UIRResolved *index_resolved =
        analyser->resolved_mapping.get(inst->index.index);
    checkResolved(index_resolved);

    RIRTypeId usize_type = analyser->rir_ctx->types->push(
        {.kind = RIRTypeKind::Integer, .integer = {false, -1}});
    RIRValueId resolved_index_id =
        getInstFromResolved(analyser, index_resolved, usize_type);
    RIRValue *resolved_index_inst =
        analyser->rir_ctx->getInst(resolved_index_id);

    // Compare
    RIRType *index_type =
        analyser->rir_ctx->getType(resolved_index_inst->result);
    expect(index_type->kind == RIRTypeKind::Integer &&
               !index_type->integer.is_signed && index_type->integer.bits == -1,
           index_inst->source_location, "Index must be of type `usize`");

    // Create Instruction
    RIRTypeId result = analyser->rir_ctx->types->push(
        {.kind = RIRTypeKind::Pointer,
         .child = resolved_ptr_child_type->slice.child});
    RIRValueId out_id = analyser->builder.buildIndex(
        resolved_ptr_inst->id, resolved_index_inst->id, result);
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::Range: {
    UIRValue *ptr_inst = analyser->ctx->getInst(inst->range.ptr);
    UIRValue *start_inst = analyser->ctx->getInst(inst->range.start);
    UIRValue *end_inst = analyser->ctx->getInst(inst->range.end);
    analyse(analyser, ptr_inst);
    analyse(analyser, start_inst);
    analyse(analyser, end_inst);

    // Check Pointer
    UIRResolved *ptr_resolved = analyser->resolved_mapping.get(inst->range.ptr);
    checkResolved(ptr_resolved);
    assert(ptr_resolved->kind == UIRResolvedKind::Inst);

    RIRValue *resolved_ptr_inst =
        analyser->rir_ctx->getInst(ptr_resolved->inst);
    RIRType *resolved_ptr_type =
        analyser->rir_ctx->getType(resolved_ptr_inst->result);
    RIRType *resolved_ptr_child_type =
        analyser->rir_ctx->getType(resolved_ptr_type->child);
    expect(resolved_ptr_child_type->kind == RIRTypeKind::Slice ||
               resolved_ptr_child_type->kind == RIRTypeKind::Pointer,
           ptr_inst->source_location, "Cannot range into non-slice/pointer");

    // Check Index and Length
    RIRTypeId usize_ty = analyser->rir_ctx->types->push({
        .kind = RIRTypeKind::Integer,
        .integer = {false, -1},
    });

    UIRResolved *start_resolved =
        analyser->resolved_mapping.get(inst->range.start);
    UIRResolved *end_resolved = analyser->resolved_mapping.get(inst->range.end);
    checkResolved(start_resolved);
    checkResolved(end_resolved);

    RIRValueId resolved_start_id =
        getInstFromResolved(analyser, start_resolved, usize_ty);
    RIRValueId resolved_end_id =
        getInstFromResolved(analyser, end_resolved, usize_ty);
    RIRValue *resolved_start_inst =
        analyser->rir_ctx->getInst(resolved_start_id);
    RIRValue *resolved_end_inst = analyser->rir_ctx->getInst(resolved_end_id);

    // Check types
    RIRType *resolved_start_type =
        analyser->rir_ctx->getType(resolved_start_inst->result);
    expect(resolved_start_type->kind == RIRTypeKind::Integer &&
               !resolved_start_type->integer.is_signed &&
               resolved_start_type->integer.bits == -1,
           start_inst->source_location, "Range start must be `usize`");

    RIRType *end_type = analyser->rir_ctx->getType(resolved_end_inst->result);
    expect(end_type->kind == RIRTypeKind::Integer &&
               !end_type->integer.is_signed && end_type->integer.bits == -1,
           end_inst->source_location, "Range end must be `usize`");

    // Create Instruction
    RIRTypeId elem_type;
    if (resolved_ptr_child_type->kind == RIRTypeKind::Pointer) {
      elem_type = resolved_ptr_child_type->child;
    } else {
      elem_type = resolved_ptr_child_type->slice.child;
    }

    RIRValueId length_id =
        analyser->builder.buildBinOp(RIROpcode::Sub, resolved_end_inst->id,
                                     resolved_start_inst->id, usize_ty);
    RIRValueId out_id = analyser->builder.buildRange(
        resolved_ptr_inst->id, resolved_start_inst->id, length_id, elem_type);
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::LookupPtr: {
    analyseLookupPtr(analyser, inst);
    break;
  }
  case UIRValueKind::LookupValue: {
    analyseLookupValue(analyser, inst);
    break;
  }
  case UIRValueKind::Aggregate: {
    analyseAggregate(analyser, inst);
    break;
  }
  case UIRValueKind::Return: {
    UIRValue *parent_inst = analyser->ctx->getInst(inst->parent.get());
    expect(parent_inst->kind == UIRValueKind::Function, inst->source_location,
           "A runtime return must be a child of a function");

    UIRResolved *function_resolved =
        analyser->resolved_mapping.get(parent_inst->id);
    checkResolved(function_resolved);
    assert(function_resolved->kind == UIRResolvedKind::Inst);

    RIRValue *function = analyser->rir_ctx->getInst(function_resolved->inst);
    RIRType *fn_type = analyser->rir_ctx->getType(function->result);
    RIRType *expected_type =
        analyser->rir_ctx->getType(fn_type->function._return);

    Option<RIRValueId> ret_value = {};
    if (inst->ret.value.isNone()) {
      expect(expected_type->kind != RIRTypeKind::Void, inst->source_location,
             "Function expects return value");
    } else {
      UIRValue *uir_value_inst = analyser->ctx->getInst(inst->ret.value.get());
      analyse(analyser, uir_value_inst);
      UIRResolved *value_resolved =
          analyser->resolved_mapping.get(uir_value_inst->id);
      checkResolved(value_resolved);

      RIRValueId resolved_value_id =
          getInstFromResolved(analyser, value_resolved, expected_type->id);
      RIRValue *resolved_value_inst =
          analyser->rir_ctx->getInst(resolved_value_id);

      // Cast and Compare
      resolved_value_inst =
          autoCast(analyser, resolved_value_inst, expected_type->id);
      expect(expected_type->compare(analyser->rir_ctx->types,
                                    resolved_value_inst->result),
             uir_value_inst->source_location,
             "Unexpected return type. Got `" << resolved_value_inst->result
                                             << "` Expected `"
                                             << expected_type->id << "`");

      ret_value.setSome(resolved_value_inst->id);
    }

    // Create Instruction
    RIRValueId out_id = analyser->builder.buildReturn(ret_value);
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::Branch: {
    RIRBlockId *dest =
        analyser->resolved_block_mapping.get(inst->br); // FIXME: Confirm exists
    RIRValueId out_id = analyser->builder.buildBranch(*dest);
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::CondBranch: {
    UIRValue *condition_inst = analyser->ctx->getInst(inst->condbr.condition);
    analyse(analyser, condition_inst);

    UIRResolved *condition_resolved =
        analyser->resolved_mapping.get(inst->condbr.condition);
    checkResolved(condition_resolved);

    RIRValue *resolved_condition =
        analyser->rir_ctx->getInst(condition_resolved->inst);
    RIRType *condition_type =
        analyser->rir_ctx->getType(resolved_condition->result);
    expect(condition_type->kind == RIRTypeKind::Bool, inst->source_location,
           "Conditional must be Bool");

    // Get Blocks
    RIRBlockId *then = analyser->resolved_block_mapping.get(
        inst->condbr.then); // FIXME: Confirm exists
    RIRBlockId *_else = analyser->resolved_block_mapping.get(
        inst->condbr._else); // FIXME: Confirm exists

    // Create Instruction
    RIRValueId out_id = analyser->builder.buildCondBranch(
        resolved_condition->id, *then, *_else);
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }
  case UIRValueKind::Switch: {
    UIRValue *condition_inst = analyser->ctx->getInst(inst->condbr.condition);
    analyse(analyser, condition_inst);
    UIRResolved *condition_resolved = analyser->resolved_mapping.get(
        inst->_switch.condition); // FIXME: Confirm kind
    checkResolved(condition_resolved);
    // TODO: Eliminate dead-code if `condition_id` is a literal

    RIRType *expected_type = analyser->rir_ctx->getType(
        analyser->rir_ctx->getInst(condition_resolved->inst)->result);
    expect(expected_type->kind == RIRTypeKind::Integer ||
               expected_type->kind == RIRTypeKind::Enum,
           condition_inst->source_location,
           "Condtional must be Integer or Enum");

    RIRBlockId *default_dest = analyser->resolved_block_mapping.get(
        inst->_switch.default_block); // FIXME: Confirm exists

    // Create Switch Instruction
    RIRValueId out_id = analyser->builder.buildSwitch(
        condition_resolved->inst, *default_dest, inst->_switch.onvals.len);
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Inst, .inst = out_id});

    // Create Cases
    for (size_t i = 0; i < inst->_switch.onvals.len; i++) {
      // Get Constant
      UIRValue *on_val = analyser->ctx->getInst(inst->_switch.onvals.ptr[i]);
      analyse(analyser, on_val);

      UIRLiteral onval_literal = analyser->comptime_state.execute(on_val);
      if (onval_literal.lit_type.isSome()) {
        expect(expected_type->compare(analyser->rir_ctx->types,
                                      onval_literal.lit_type.get()),
               on_val->source_location,
               "Switch On_Val type `" << onval_literal.lit_type.get()
                                      << "` doesn't match condition type `"
                                      << expected_type->id << "`");
      }

      RIRValueId constant_id = analyser->builder.buildConstant(
          expected_type->id, {.kind = RIRConstantKind::Integer,
                              .integer = onval_literal.data._int});

      // Add Case
      RIRBlockId *dest = analyser->resolved_block_mapping.get(
          inst->_switch.blocks.ptr[i]); // FIXME: Confirm exists

      analyser->builder.addCase(out_id, constant_id, *dest, i);
    }
    break;
  }
  case UIRValueKind::Assembly: {
    Slice<RIRAssembly> instructions = {
        .ptr = (RIRAssembly *)analyser->allocator->alloc(sizeof(RIRAssembly) *
                                                         inst->assembly.len),
        .len = inst->assembly.len,
    };

    for (size_t i = 0; i < inst->assembly.len; i++) {
      UIRAssembly *asm_inst = inst->assembly.ptr + i;
      RIRAssembly *out_inst = instructions.ptr + i;
      out_inst->name = asm_inst->name;
      out_inst->operands = {
          .ptr = (RIRAssembly::Operand *)analyser->allocator->alloc(
              sizeof(RIRAssembly::Operand) * asm_inst->operands.len),
          .len = asm_inst->operands.len,
      };

      for (size_t o = 0; o < asm_inst->operands.len; o++) {
        UIRAssembly::Operand *uir_operand = asm_inst->operands.ptr + o;
        RIRAssembly::Operand *rir_operand = out_inst->operands.ptr + o;
        if (uir_operand->kind == UIRAssembly::Operand::Register) {
          rir_operand->kind = RIRAssembly::Operand::Register;
          rir_operand->reg = uir_operand->reg;
          continue;
        }

        UIRResolved *operand_resolved =
            analyser->resolved_mapping.get(uir_operand->uir);
        checkResolved(operand_resolved);

        rir_operand->rir_inst =
            getInstFromResolved(analyser, operand_resolved, {});
        if (uir_operand->kind == UIRAssembly::Operand::Input) {
          rir_operand->kind = RIRAssembly::Operand::Input;
        } else if (uir_operand->kind == UIRAssembly::Operand::Return) {
          rir_operand->kind = RIRAssembly::Operand::Return;
        }
      }
    }

    // Create Instruction
    RIRValueId out_id = analyser->builder.buildAssembly(instructions);
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    break;
  }

  case UIRValueKind::Comptime: {
    UIRLiteral literal = analyser->comptime_state.execute(inst);

    switch (literal.data.kind) {
    case UIRRawDataKind::Void:
    case UIRRawDataKind::TypeId:
    case UIRRawDataKind::Namespace: {
      analyser->resolved_mapping.insert(
          inst->id, {.kind = UIRResolvedKind::Literal, .literal = literal});
      return;
    }
    }

    // Create Instruction
    if (literal.lit_type.isSome()) {
      RIRConstant constant =
          uirRawDataToRIRConstant(analyser->allocator, literal.data);
      RIRValueId out_id =
          analyser->builder.buildConstant(literal.lit_type.get(), constant);

      analyser->resolved_mapping.insert(
          inst->id, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    } else {
      analyser->resolved_mapping.insert(
          inst->id, {.kind = UIRResolvedKind::Literal, .literal = literal});
    }
    break;
  }
  case UIRValueKind::TypeOf: {
    UIRLiteral literal = analyser->comptime_state.execute(inst);
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Literal, .literal = literal});
    break;
  }

  case UIRValueKind::GlobalVariable:
  case UIRValueKind::Function: {
    analyseGlobal(analyser, inst);
    break;
  }
  case UIRValueKind::Literal: {
    UIRLiteral literal = inst->literal;
    RIRConstant constant =
        uirRawDataToRIRConstant(analyser->allocator, literal.data);

    // Create Instruction
    if (literal.lit_type.isSome()) {
      RIRValueId out_id =
          analyser->builder.buildConstant(literal.lit_type.get(), constant);
      analyser->resolved_mapping.insert(
          inst->id, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    } else {
      analyser->resolved_mapping.insert(
          inst->id, {.kind = UIRResolvedKind::Literal, .literal = literal});
    }
    break;
  }
  case UIRValueKind::Struct: {
    RIRTypeId struct_type_id =
        analyser->ctx->types->push({.kind = RIRTypeKind::Struct});

    // Pre-populate resolved
    UIRLiteral lit = {
        .data = {.kind = UIRRawDataKind::TypeId, ._typeid = struct_type_id}};
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Literal, .literal = lit});

    // Compute type
    analyser->comptime_state.pushStack();
    computeStructType(&analyser->comptime_state, inst, struct_type_id);
    analyser->comptime_state.popStack();

    // Analyse definitions
    analyseScope(analyser, analyser->ctx->getScope(inst->_struct.definitions));
    break;
  }
  case UIRValueKind::Enum: {
    RIRTypeId enum_type_id =
        analyser->ctx->types->push({.kind = RIRTypeKind::Enum});

    // Pre-populate resolved
    UIRLiteral lit = {
        .data = {.kind = UIRRawDataKind::TypeId, ._typeid = enum_type_id}};
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Literal, .literal = lit});

    // Compute type
    analyser->comptime_state.pushStack();
    computeEnumType(&analyser->comptime_state, inst, enum_type_id);
    analyser->comptime_state.popStack();

    // Analyse definitions
    analyseScope(analyser, analyser->ctx->getScope(inst->_enum.definitions));
    break;
  }
  case UIRValueKind::Union: {
    RIRTypeId union_type_id =
        analyser->ctx->types->push({.kind = RIRTypeKind::Union});

    // Pre-populate resolved
    UIRLiteral lit = {
        .data = {.kind = UIRRawDataKind::TypeId, ._typeid = union_type_id}};
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Literal, .literal = lit});

    // Compute type
    analyser->comptime_state.pushStack();
    computeUnionType(&analyser->comptime_state, inst, union_type_id);
    analyser->comptime_state.popStack();

    // Analyse definitions
    analyseScope(analyser, analyser->ctx->getScope(inst->_enum.definitions));
    break;
  }
  case UIRValueKind::Namespace: {
    UIRLiteral lit = analyser->comptime_state.execute(inst);
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Literal, .literal = lit});

    // Analyse definitions
    analyseScope(analyser,
                 analyser->ctx->getScope(inst->_namespace.definitions));
    break;
  }
  default: {
    std::cerr << "TODO: Implement analysis of `" << std::hex
              << (uint16_t)inst->kind << "`\n";
    std::abort();
    break;
  }
  }

  analyser->builder.module = prev_mod;
}

void analyseBlock(UIRAnalyser *analyser, UIRBlock *block) {
  RIRModule *prev_mod = analyser->builder.module;
  analyser->builder.module =
      analyser->rir_ctx->modules.getPtr(block->id.module);

  for (size_t i = 0; i < block->instructions.length; i++) {
    UIRValueId inst_id = block->instructions.getUnchecked(i);
    analyse(analyser, analyser->ctx->getInst(inst_id));
  }

  analyser->builder.module = prev_mod;
}

void analyseScope(UIRAnalyser *analyser, UIRScope *scope) {
  RIRModule *prev_mod = analyser->builder.module;
  analyser->builder.module =
      analyser->rir_ctx->modules.getPtr(scope->id.module);

  for (size_t i = 0; i < scope->list.length; i++) {
    UIRValueId inst_id = scope->list.getUnchecked(i);
    analyse(analyser, analyser->ctx->getInst(inst_id));
  }

  analyser->builder.module = prev_mod;
}

RIRContext *UIRAnalyser::analyse() {
  for (size_t i = 0; i < this->ctx->modules.len(); i++) {
    UIRModule *uir_mod = this->ctx->modules.getPtrUnchecked(i);
    this->builder.module = this->rir_ctx->modules.getPtrUnchecked(i);

    analyseScope(this, this->ctx->getScope(uir_mod->definitions));
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
  this->type_extras.init(allocator, 512);
  this->mangled_name_cache.init(allocator, 128);

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
  this->type_extras.deinit();
  this->mangled_name_cache.deinit();
}
