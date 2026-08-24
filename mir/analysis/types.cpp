#include "analysis/analysis.hpp"
#include "define.hpp"
#include "mir.hpp"

bool compareTypes(Type *lhs, Type *rhs) {
  if (lhs->kind != rhs->kind) {
    return false;
  }

  switch (lhs->kind) {
  case TypeKind::Void: {
    return true;
  }
  case TypeKind::Bool: {
    return true;
  }
  case TypeKind::Integer: {
    bool term1 = lhs->integer.is_untyped || lhs->integer.is_signed ||
                 !rhs->integer.is_signed;
    bool term2 = rhs->integer.is_untyped || rhs->integer.is_signed ||
                 !lhs->integer.is_signed;

    bool bits_match = lhs->integer.bits == rhs->integer.bits;
    bool untyped_or_bits =
        lhs->integer.is_untyped || rhs->integer.is_untyped || bits_match;

    return term1 && term2 && untyped_or_bits;
  }
  case TypeKind::Float: {
    return lhs->_float.is_untyped || rhs->_float.is_untyped ||
           lhs->_float.bits == rhs->_float.bits;
  }
  case TypeKind::Pointer: {
    return compareTypes(lhs->child, rhs->child);
  }
  case TypeKind::Slice:
  case TypeKind::SIMD: {
    return lhs->slice.length == rhs->slice.length &&
           compareTypes(lhs->slice.type, rhs->slice.type);
  }
  case TypeKind::TypeId: {
    return true;
  }
  case TypeKind::Function: {
    if (lhs->function.arguments.len != rhs->function.arguments.len) {
      return false;
    }

    for (size_t i = 0; i < lhs->function.arguments.len; i++) {
      if (!compareTypes(lhs->function.arguments.ptr[i],
                        rhs->function.arguments.ptr[i])) {
        return false;
      }
    }

    return compareTypes(lhs->function.return_type, rhs->function.return_type);
  }
  case TypeKind::Struct: {
    return lhs->_struct.scope == rhs->_struct.scope;
  }
  case TypeKind::Enum: {
    return compareTypes(lhs->_enum.repr_type, rhs->_enum.repr_type);
  }
  case TypeKind::Union: {
    return lhs->_union.scope == rhs->_union.scope;
  }
  }

  return false;
}

// Attempts to NO-OP convert `src` to `dst`
Type *autoConvert(MIRAnalyser *analyser, Type *src, Type *dst) {
  if (src->kind == TypeKind::Integer && src->integer.is_untyped &&
      dst->kind == TypeKind::Integer) {
    if (dst->integer.is_signed || !src->integer.is_signed) {
      return dst;
    }
  } else if (src->kind == TypeKind::Float && src->_float.is_untyped &&
             dst->kind == TypeKind::Float) {
    return dst;
  } else if (src->kind == TypeKind::Pointer && dst->kind == TypeKind::Slice &&
             dst->slice.length < 0 &&
             compareTypes(src->child, dst->slice.type)) {
    // Pointer to Pointer Slice
    return dst;
  }

  return src;
}

void fixUntyped(MIRAnalyser *analyser, MIRValue *inst, Type *real) {
  if (!(inst->result_type->kind == TypeKind::Integer &&
        inst->result_type->integer.is_untyped) &&
      !(inst->result_type->kind == TypeKind::Float &&
        inst->result_type->_float.is_untyped)) {
    return;
  }

  inst->result_type = real;
  switch (inst->kind) {
  case MIRValueKind::BinOp: {
    fixUntyped(analyser, inst->binop.lhs, real);
    fixUntyped(analyser, inst->binop.rhs, real);
    break;
  }
  case MIRValueKind::UnaryOp: {
    fixUntyped(analyser, inst->unaryop.value, real);
    break;
  }
  }
}

void autoCast(MIRAnalyser *analyser, MIRValue *src, Type *dst) {
  if ((src->result_type->kind == TypeKind::Integer &&
       src->result_type->integer.is_untyped) ||
      (src->result_type->kind == TypeKind::Float &&
       src->result_type->_float.is_untyped)) {
    fixUntyped(analyser, src, dst);
  }

  src->result_type = autoConvert(analyser, src->result_type, dst);
}
