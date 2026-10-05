#include "define.hpp"
#include "rir/constant.hpp"
#include "rir/rir.hpp"
#include "rir/type.hpp"
#include "uir/analysis/analysis.hpp"
#include "uir/literal.hpp"

void analyseBinary(UIRAnalyser *analyser, UIRModule *module, UIRValue *inst) {
  analyse(analyser, module, inst->binop.lhs);
  analyse(analyser, module, inst->binop.rhs);
  UIRResolved *lhs_resolved = analyser->resolved_mapping.get(inst->binop.lhs);
  UIRResolved *rhs_resolved = analyser->resolved_mapping.get(inst->binop.rhs);

  // Execute comptime
  if (lhs_resolved->kind == UIRResolvedKind::Literal &&
      rhs_resolved->kind == UIRResolvedKind::Literal) {
    UIRLiteral lit = analyser->comptime_state.execute(module, inst);
    analyser->resolved_mapping.insert(
        inst, {.kind = UIRResolvedKind::Literal, .literal = lit});
    return;
  }

  // Get operand instructions
  RIRValue *lhs;
  RIRValue *rhs;
  RIRType *lhs_type;
  RIRType *rhs_type;
  if (lhs_resolved->kind == UIRResolvedKind::Inst) {
    lhs = analyser->rir_ctx->getInst(lhs_resolved->inst);
    lhs_type = analyser->rir_ctx->getType(lhs->result);
  }
  if (rhs_resolved->kind == UIRResolvedKind::Inst) {
    rhs = analyser->rir_ctx->getInst(rhs_resolved->inst);
    rhs_type = analyser->rir_ctx->getType(rhs->result);
  }

  if (lhs_resolved->kind == UIRResolvedKind::Literal) {
    // Make literal typed
    expect(compareRawDataToType(analyser, lhs_resolved->literal.data.kind,
                                rhs->result),
           inst->binop.lhs->source_location, "LHS literal must match RHS type");

    lhs_type = rhs_type;
    RIRConstant rir_const = uirRawDataToRIRConstant(analyser->allocator,
                                                    lhs_resolved->literal.data);
    RIRValueId lhs_id = analyser->builder.buildConstant(rhs->result, rir_const);
    lhs = analyser->rir_ctx->getInst(lhs_id);
  } else if (rhs_resolved->kind == UIRResolvedKind::Literal) {
    if (rhs_resolved->literal.data.kind == UIRRawDataKind::TypeId) {
      // Casting
      switch (inst->binop.opcode) {
      case UIROpcode::As:
      case UIROpcode::Bitcast: {
        expect(lhs_type->kind != RIRTypeKind::TypeId,
               inst->binop.lhs->source_location, "LHS must not be a type");
        expect(rhs_resolved->literal.data.kind == UIRRawDataKind::TypeId,
               inst->binop.rhs->source_location, "RHS must be a type");

        // `As` cast restrictions
        if (inst->binop.opcode == UIROpcode::Bitcast) {
          RIRValueId out_id = analyser->builder.buildCast(
              lhs->id, rhs_resolved->literal.data._typeid, true);
          analyser->resolved_mapping.insert(
              inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
          break;
        }

        RIRType *src_type = lhs_type;
        RIRType *dst_type =
            analyser->rir_ctx->getType(rhs_resolved->literal.data._typeid);
        bool allowed = false;

        if (src_type->kind == RIRTypeKind::Bool) {
          allowed = (dst_type->kind == RIRTypeKind::Bool ||
                     dst_type->kind == RIRTypeKind::Integer ||
                     dst_type->kind == RIRTypeKind::Float);
        } else if (src_type->kind == RIRTypeKind::Integer) {
          allowed = dst_type->kind == RIRTypeKind::Integer ||
                    dst_type->kind == RIRTypeKind::Float ||
                    dst_type->kind == RIRTypeKind::Pointer;
        } else if (src_type->kind == RIRTypeKind::Float) {
          allowed = dst_type->kind == RIRTypeKind::Integer ||
                    dst_type->kind == RIRTypeKind::Float;
        } else if (src_type->kind == RIRTypeKind::Pointer) {
          allowed = dst_type->kind == RIRTypeKind::Integer &&
                    !dst_type->integer.is_signed &&
                    dst_type->integer.bits == -1;
        } else if (src_type->kind == RIRTypeKind::Slice) {
          if (src_type->slice.length == 0 && dst_type->slice.length == 0) {
            allowed = true; // No-Op
          } else if (src_type->slice.length > 0 &&
                     dst_type->slice.length == 0) {
            allowed = true; // Compile-time to Runtime
          }

          RIRType *src_elem_type =
              analyser->rir_ctx->getType(src_type->slice.child);
          allowed &= src_elem_type->compare(analyser->rir_ctx->types,
                                            dst_type->slice.child);
        } else if (src_type->kind == RIRTypeKind::Enum) {
          allowed = dst_type->kind == RIRTypeKind::Integer;
        }

        expect(allowed, inst->source_location,
               "Cannot `as` cast `" << src_type << "` to `" << dst_type << "`");

        RIRValueId out_id = analyser->builder.buildCast(
            lhs->id, rhs_resolved->literal.data._typeid, false);
        analyser->resolved_mapping.insert(
            inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
        break;
      }
      }
      return;
    }

    // Make literal typed
    expect(compareRawDataToType(analyser, rhs_resolved->literal.data.kind,
                                lhs->result),
           inst->binop.lhs->source_location, "RHS literal must match LHS type");

    rhs_type = lhs_type;
    RIRConstant rir_const = uirRawDataToRIRConstant(analyser->allocator,
                                                    rhs_resolved->literal.data);
    RIRValueId rhs_id = analyser->builder.buildConstant(lhs->result, rir_const);
    rhs = analyser->rir_ctx->getInst(rhs_id);
  }

  // Get primitive type
  RIRType *lhs_primitive = lhs_type;
  RIRType *rhs_primitive = rhs_type;
  RIRValueId out_id;

  switch (inst->binop.opcode) {
  case UIROpcode::Add:
  case UIROpcode::Sub:
  case UIROpcode::Mul:
  case UIROpcode::Div:
  case UIROpcode::Mod: {
    if (lhs_primitive->kind != RIRTypeKind::Integer &&
        lhs_primitive->kind != RIRTypeKind::Float &&
        lhs_primitive->kind != RIRTypeKind::Pointer) {
      expect(false, inst->binop.lhs->source_location,
             "LHS must be of Integer, Float, Pointer, or SIMD.");
    }

    expect(lhs_primitive->compare(analyser->rir_ctx->types, rhs_primitive->id),
           inst->binop.rhs->source_location, "LHS cannot operate with RHS");

    out_id = analyser->builder.buildBinOp((RIROpcode)inst->binop.opcode,
                                          lhs->id, rhs->id, lhs->result);
    break;
  }
  case UIROpcode::Or:
  case UIROpcode::And: {
    expect(lhs_primitive->kind == RIRTypeKind::Integer ||
               lhs_primitive->kind == RIRTypeKind::Bool,
           inst->binop.lhs->source_location,
           "LHS must be a Bool or Integer. Got `" << lhs_primitive << "`");
    expect(lhs_primitive->compare(analyser->rir_ctx->types, rhs_primitive->id),
           inst->binop.rhs->source_location,
           "LHS `" << lhs_primitive << "` cannot operate with RHS `"
                   << rhs_primitive << "`");

    out_id = analyser->builder.buildBinOp((RIROpcode)inst->binop.opcode,
                                          lhs->id, rhs->id, lhs->result);
    break;
  }
  case UIROpcode::Xor:
  case UIROpcode::LeftShift:
  case UIROpcode::RightShift: {
    expect(lhs_primitive->kind == RIRTypeKind::Integer,
           inst->binop.lhs->source_location,
           "LHS must be an Integer. Got `" << lhs_primitive << "`");
    expect(lhs_primitive->compare(analyser->rir_ctx->types, rhs_primitive->id),
           inst->binop.rhs->source_location,
           "LHS `" << lhs_primitive << "` cannot operate with RHS `"
                   << rhs_primitive << "`");

    out_id = analyser->builder.buildBinOp((RIROpcode)inst->binop.opcode,
                                          lhs->id, rhs->id, lhs->result);
    break;
  }
  case UIROpcode::EqualTo:
  case UIROpcode::NotEqualTo: {
    expect(lhs_primitive->compare(analyser->rir_ctx->types, rhs_primitive->id),
           inst->binop.rhs->source_location,
           "LHS `" << lhs_primitive << "` cannot operate with RHS `"
                   << rhs_primitive << "`");

    RIRTypeId result =
        analyser->rir_ctx->types->push({.kind = RIRTypeKind::Bool});
    out_id = analyser->builder.buildBinOp((RIROpcode)inst->binop.opcode,
                                          lhs->id, rhs->id, result);
    break;
  }
  case UIROpcode::LessThen:
  case UIROpcode::GreaterThen:
  case UIROpcode::LessThenOrEqualTo:
  case UIROpcode::GreaterThenOrEqualTo: {
    if (lhs_primitive->kind != RIRTypeKind::Integer &&
        lhs_primitive->kind != RIRTypeKind::Float) {
      expect(false, inst->binop.lhs->source_location,
             "LHS must be of Integer, Float, or SIMD. Got `" << lhs_primitive
                                                             << "`");
    }

    expect(lhs_primitive->compare(analyser->rir_ctx->types, rhs_primitive->id),
           inst->binop.rhs->source_location,
           "LHS `" << lhs_primitive << "` cannot operate with RHS `"
                   << rhs_primitive << "`");

    RIRTypeId result =
        analyser->rir_ctx->types->push({.kind = RIRTypeKind::Bool});
    out_id = analyser->builder.buildBinOp((RIROpcode)inst->binop.opcode,
                                          lhs->id, rhs->id, result);
    break;
  }
  }

  analyser->resolved_mapping.insert(
      inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
}

void analyseUnary(UIRAnalyser *analyser, UIRModule *module, UIRValue *inst) {
  analyse(analyser, module, inst->unaryop.value);
  UIRResolved *child_resolved =
      analyser->resolved_mapping.get(inst->unaryop.value);

  // Comptime
  if (child_resolved->kind == UIRResolvedKind::Literal) {
    UIRLiteral lit = analyser->comptime_state.execute(module, inst);

    if (lit.lit_type.isSome()) {
      RIRConstant rir_const =
          uirRawDataToRIRConstant(analyser->allocator, lit.data);
      RIRValueId out_id =
          analyser->builder.buildConstant(lit.lit_type.get(), rir_const);
      analyser->resolved_mapping.insert(
          inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    } else {
      analyser->resolved_mapping.insert(
          inst, {.kind = UIRResolvedKind::Literal, .literal = lit});
    }
    return;
  }

  RIRValue *child = analyser->rir_ctx->getInst(child_resolved->inst);
  RIRType *child_primitive = analyser->rir_ctx->getType(child->result);

  RIRValueId out_id;

  switch (inst->unaryop.opcode) {
  case UIROpcode::Minus: {
    if (child_primitive->kind != RIRTypeKind::Integer &&
        child_primitive->kind != RIRTypeKind::Float) {
      expect(false, inst->unaryop.value->source_location,
             "Child must be of Integer, Float, or SIMD. Got `"
                 << child_primitive << "`");
    }

    RIRTypeId result = child->result;
    if (child_primitive->kind == RIRTypeKind::Integer &&
        !child_primitive->integer.is_signed) {
      RIRType ty = *child_primitive;
      ty.integer.is_signed = true;
      result = analyser->rir_ctx->types->push(ty);
    }

    out_id = analyser->builder.buildUnaryOp((RIROpcode)inst->binop.opcode,
                                            child->id, result);
    break;
  }
  case UIROpcode::LogicalNot: {
    expect(child_primitive->kind == RIRTypeKind::Bool,
           inst->unaryop.value->source_location,
           "Child must be Bool. Got `" << child_primitive << "`");
    out_id = analyser->builder.buildUnaryOp((RIROpcode)inst->binop.opcode,
                                            child->id, child->result);
    break;
  }
  case UIROpcode::BitwiseNot: {
    expect(child_primitive->kind == RIRTypeKind::Integer,
           inst->unaryop.value->source_location,
           "Child must be Integer. Got `" << child_primitive << "`");

    out_id = analyser->builder.buildUnaryOp((RIROpcode)inst->binop.opcode,
                                            child->id, child->result);
    break;
  }
  }

  analyser->resolved_mapping.insert(
      inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
}
