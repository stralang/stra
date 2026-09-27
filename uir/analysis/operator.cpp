#include "define.hpp"
#include "rir/rir.hpp"
#include "rir/type.hpp"
#include "uir/analysis/analysis.hpp"
#include "uir/literal.hpp"

void analyseBinary(UIRAnalyser *analyser, UIRModule *module, UIRValue *inst) {
  analyse(analyser, module, inst->binop.lhs);
  analyse(analyser, module, inst->binop.rhs);
  UIRResolved *lhs_id = analyser->resolved_mapping.get(inst->binop.lhs);
  UIRResolved *rhs_id = analyser->resolved_mapping.get(inst->binop.rhs);

  expect(
      lhs_id->kind == UIRResolvedKind::Inst, inst->binop.rhs->source_location,
      "!UIR Internal! cannot generate binary operation with comptime literal");
  RIRValue *lhs = analyser->rir_ctx->getInst(lhs_id->inst);
  RIRType *lhs_type = analyser->rir_ctx->getType(lhs->result);

  // Casting
  if (rhs_id->kind == UIRResolvedKind::Literal &&
      rhs_id->literal.data.kind == UIRRawDataKind::TypeId) {
    switch (inst->binop.opcode) {
    case UIROpcode::As:
    case UIROpcode::Bitcast: {
      expect(lhs_type->kind != RIRTypeKind::TypeId,
             inst->binop.lhs->source_location, "LHS must not be a type");
      expect(rhs_id->literal.data.kind == UIRRawDataKind::TypeId,
             inst->binop.rhs->source_location, "RHS must be a type");

      // `As` cast restrictions
      if (inst->binop.opcode == UIROpcode::Bitcast) {
        RIRValueId out_id = analyser->builder.buildCast(
            lhs_id->inst, rhs_id->literal.data._typeid, true);
        analyser->resolved_mapping.insert(
            inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
        break;
      }

      RIRType *src_type = lhs_type;
      RIRType *dst_type =
          analyser->rir_ctx->getType(rhs_id->literal.data._typeid);
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
                  !dst_type->integer.is_untyped &&
                  !dst_type->integer.is_signed && dst_type->integer.bits == -1;
      } else if (src_type->kind == RIRTypeKind::Slice) {
        if (src_type->slice.length == 0 && dst_type->slice.length == 0) {
          allowed = true; // No-Op
        } else if (src_type->slice.length > 0 && dst_type->slice.length == 0) {
          allowed = true; // Compile-time to Runtime
        }

        // FIXME: allowed &= compareTypes(src_type->slice.type,
        // dst_type->slice.type);
      } else if (src_type->kind == RIRTypeKind::Enum) {
        allowed = dst_type->kind == RIRTypeKind::Integer;
      }

      expect(allowed, inst->source_location,
             "Cannot `as` cast `" << src_type << "` to `" << dst_type << "`");

      RIRValueId out_id = analyser->builder.buildCast(
          lhs_id->inst, rhs_id->literal.data._typeid, false);
      analyser->resolved_mapping.insert(
          inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
      break;
    }
    }
    return;
  }

  expect(
      rhs_id->kind == UIRResolvedKind::Inst, inst->binop.rhs->source_location,
      "!UIR Internal! cannot generate binary operation with comptime literal");
  RIRValue *rhs = analyser->rir_ctx->getInst(rhs_id->inst);
  RIRType *rhs_type = analyser->rir_ctx->getType(rhs->result);

  // FIXME: Convert from untyped
  // if (lhs->result_type->kind == rhs->result_type->kind) {
  //   bool lhs_untyped = false;
  //   bool rhs_untyped = false;
  //   if (lhs->result_type->kind == TypeKind::Integer) {
  //     lhs_untyped = lhs->result_type->integer.is_untyped;
  //   } else if (lhs->result_type->kind == TypeKind::Float) {
  //     lhs_untyped = lhs->result_type->_float.is_untyped;
  //   }
  //   if (rhs->result_type->kind == TypeKind::Integer) {
  //     rhs_untyped = rhs->result_type->integer.is_untyped;
  //   } else if (rhs->result_type->kind == TypeKind::Float) {
  //     rhs_untyped = rhs->result_type->_float.is_untyped;
  //   }
  //
  //   if (lhs_untyped && !rhs_untyped) {
  //     fixUntyped(analyser, lhs, rhs->result_type);
  //   } else if (!lhs_untyped && rhs_untyped) {
  //     fixUntyped(analyser, rhs, lhs->result_type);
  //   }
  // }

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

    // FIXME: expect(compareTypes(lhs_primitive, rhs_primitive),
    // inst->binop.rhs->source_location,
    //        "LHS cannot operate with RHS");

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
    // FIXME: expect(compareTypes(lhs_primitive, rhs_primitive),
    // rhs->source_location,
    //        "LHS `" << lhs_primitive << "` cannot operate with RHS `"
    //                << rhs_primitive << "`");

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
    // FIXME: expect(compareTypes(lhs_primitive, rhs_primitive),
    // rhs->source_location,
    //        "LHS `" << lhs_primitive << "` cannot operate with RHS `"
    //                << rhs_primitive << "`");

    out_id = analyser->builder.buildBinOp((RIROpcode)inst->binop.opcode,
                                          lhs->id, rhs->id, lhs->result);
    break;
  }
  case UIROpcode::EqualTo:
  case UIROpcode::NotEqualTo: {
    // FIXME: expect(compareTypes(lhs_primitive, rhs_primitive),
    // rhs->source_location,
    //        "LHS `" << lhs_primitive << "` cannot operate with RHS `"
    //                << rhs_primitive << "`");

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

    // FIXME: expect(compareTypes(lhs_primitive, rhs_primitive),
    // rhs->source_location,
    //        "LHS `" << lhs_primitive << "` cannot operate with RHS `"
    //                << rhs_primitive << "`");

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
  UIRResolved *child_id = analyser->resolved_mapping.get(inst->unaryop.value);
  RIRValue *child = analyser->rir_ctx->getInst(child_id->inst);
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
