#include "define.hpp"

void analyseBinary(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst) {
  MIRValue *lhs = inst->binop.lhs;
  MIRValue *rhs = inst->binop.rhs;
  analyse(analyser, module, lhs);
  analyse(analyser, module, rhs);

  // Convert from untyped
  if (lhs->result_type->kind == rhs->result_type->kind) {
    bool lhs_untyped = false;
    bool rhs_untyped = false;
    if (lhs->result_type->kind == TypeKind::Integer) {
      lhs_untyped = lhs->result_type->integer.is_untyped;
    } else if (lhs->result_type->kind == TypeKind::Float) {
      lhs_untyped = lhs->result_type->_float.is_untyped;
    }
    if (rhs->result_type->kind == TypeKind::Integer) {
      rhs_untyped = rhs->result_type->integer.is_untyped;
    } else if (rhs->result_type->kind == TypeKind::Float) {
      rhs_untyped = rhs->result_type->_float.is_untyped;
    }

    if (lhs_untyped && !rhs_untyped) {
      fixUntyped(analyser, lhs, rhs->result_type);
    } else if (!lhs_untyped && rhs_untyped) {
      fixUntyped(analyser, rhs, lhs->result_type);
    }
  }

  // Get primitive type
  Type *lhs_primitive = lhs->result_type;
  Type *rhs_primitive = rhs->result_type;

  switch (inst->binop.opcode) {
  case MIROpcode::Add:
  case MIROpcode::Sub:
  case MIROpcode::Mul:
  case MIROpcode::Div:
  case MIROpcode::Mod: {
    if (lhs_primitive->kind != TypeKind::Integer &&
        lhs_primitive->kind != TypeKind::Float &&
        lhs_primitive->kind != TypeKind::Pointer) {
      expect(false, lhs->source_location,
             "LHS must be of Integer, Float, Pointer, or SIMD.");
    }

    expect(compareTypes(lhs_primitive, rhs_primitive), rhs->source_location,
           "LHS cannot operate with RHS");

    inst->result_type = lhs_primitive;
    break;
  }
  case MIROpcode::Or:
  case MIROpcode::And: {
    expect(lhs_primitive->kind == TypeKind::Integer ||
               lhs_primitive->kind == TypeKind::Bool,
           lhs->source_location,
           "LHS must be a Bool or Integer. Got `" << lhs_primitive << "`");
    expect(compareTypes(lhs_primitive, rhs_primitive), rhs->source_location,
           "LHS `" << lhs_primitive << "` cannot operate with RHS `"
                   << rhs_primitive << "`");

    inst->result_type = lhs_primitive;
    break;
  }
  case MIROpcode::Xor:
  case MIROpcode::LeftShift:
  case MIROpcode::RightShift: {
    expect(lhs_primitive->kind == TypeKind::Integer, lhs->source_location,
           "LHS must be an Integer. Got `" << lhs_primitive << "`");
    expect(compareTypes(lhs_primitive, rhs_primitive), rhs->source_location,
           "LHS `" << lhs_primitive << "` cannot operate with RHS `"
                   << rhs_primitive << "`");

    inst->result_type = lhs_primitive;
    break;
  }
  case MIROpcode::EqualTo:
  case MIROpcode::NotEqualTo: {
    expect(compareTypes(lhs_primitive, rhs_primitive), rhs->source_location,
           "LHS `" << lhs_primitive << "` cannot operate with RHS `"
                   << rhs_primitive << "`");

    inst->result_type = module->ctx->type_cache->get({.kind = TypeKind::Bool});
    break;
  }
  case MIROpcode::LessThen:
  case MIROpcode::GreaterThen:
  case MIROpcode::LessThenOrEqualTo:
  case MIROpcode::GreaterThenOrEqualTo: {
    if (lhs_primitive->kind != TypeKind::Integer &&
        lhs_primitive->kind != TypeKind::Float) {
      expect(false, lhs->source_location,
             "LHS must be of Integer, Float, or SIMD. Got `" << lhs_primitive
                                                             << "`");
    }

    expect(compareTypes(lhs_primitive, rhs_primitive), rhs->source_location,
           "LHS `" << lhs_primitive << "` cannot operate with RHS `"
                   << rhs_primitive << "`");
    inst->result_type = module->ctx->type_cache->get({.kind = TypeKind::Bool});
    break;
  }
  case MIROpcode::As:
  case MIROpcode::Bitcast: {
    expect(lhs_primitive->kind != TypeKind::TypeId, lhs->source_location,
           "LHS must not be a type");
    expect(rhs_primitive->kind == TypeKind::TypeId, rhs->source_location,
           "RHS must be a type");

    MIRLiteral rhs_literal = analyser->comptime_state.execute(module, rhs);
    inst->result_type = rhs_literal._typeid;

    // `As` cast restrictions
    if (inst->binop.opcode == MIROpcode::Bitcast) {
      break;
    }

    Type *src_type = lhs_primitive;
    Type *dst_type = rhs_literal._typeid;
    bool allowed = false;

    if (src_type->kind == TypeKind::Bool) {
      allowed = (dst_type->kind == TypeKind::Bool ||
                 dst_type->kind == TypeKind::Integer ||
                 dst_type->kind == TypeKind::Float);
    } else if (src_type->kind == TypeKind::Integer) {
      allowed = dst_type->kind == TypeKind::Integer ||
                dst_type->kind == TypeKind::Float ||
                dst_type->kind == TypeKind::Pointer;
    } else if (src_type->kind == TypeKind::Float) {
      allowed = dst_type->kind == TypeKind::Integer ||
                dst_type->kind == TypeKind::Float;
    } else if (src_type->kind == TypeKind::Pointer) {
      allowed = dst_type->kind == TypeKind::Integer &&
                !dst_type->integer.is_untyped && !dst_type->integer.is_signed &&
                dst_type->integer.bits == -1;
    } else if (src_type->kind == TypeKind::Slice) {
      if (src_type->slice.length == 0 && dst_type->slice.length == 0) {
        allowed = true; // No-Op
      } else if (src_type->slice.length > 0 && dst_type->slice.length == 0) {
        allowed = true; // Compile-time to Runtime
      }

      allowed &= compareTypes(src_type->slice.type, dst_type->slice.type);
    } else if (src_type->kind == TypeKind::Enum) {
      allowed = dst_type->kind == TypeKind::Integer;
    }

    expect(allowed, inst->source_location,
           "Cannot `as` cast `" << src_type << "` to `" << dst_type << "`");
    break;
  }
  }
}

void analyseUnary(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst) {
  analyse(analyser, module, inst->unaryop.value);

  Type *child_primitive = inst->unaryop.value->result_type;

  switch (inst->unaryop.opcode) {
  case MIROpcode::Minus: {
    if (child_primitive->kind != TypeKind::Integer &&
        child_primitive->kind != TypeKind::Float) {
      expect(false, inst->unaryop.value->source_location,
             "Child must be of Integer, Float, or SIMD. Got `"
                 << child_primitive << "`");
    }

    if (child_primitive->kind == TypeKind::Integer &&
        !child_primitive->integer.is_signed) {
      Type ty = *child_primitive;
      ty.integer.is_signed = true;
      inst->result_type = module->ctx->type_cache->get(ty);
    } else {
      inst->result_type = child_primitive;
    }
    break;
  }
  case MIROpcode::LogicalNot: {
    expect(child_primitive->kind == TypeKind::Bool,
           inst->unaryop.value->source_location,
           "Child must be Bool. Got `" << child_primitive << "`");
    inst->result_type = child_primitive;
    break;
  }
  case MIROpcode::BitwiseNot: {
    expect(child_primitive->kind == TypeKind::Integer,
           inst->unaryop.value->source_location,
           "Child must be Integer. Got `" << child_primitive << "`");
    inst->result_type = child_primitive;
    break;
  }
  }
}
