#include "define.hpp"
#include "evaluator.hpp"

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
    if (lhs->function.arguments.length != rhs->function.arguments.length) {
      return false;
    }

    for (size_t i = 0; i < lhs->function.arguments.length; i++) {
      if (!compareTypes(lhs->function.arguments.data.ptr[i],
                        rhs->function.arguments.data.ptr[i])) {
        return false;
      }
    }

    return compareTypes(lhs->function.return_type, rhs->function.return_type);
  }
  case TypeKind::Struct: {
    for (size_t i = 0; i < lhs->_struct.fields.length; i++) {
      if (!compareTypes(lhs->_struct.fields.data.ptr[i],
                        rhs->_struct.fields.data.ptr[i])) {
        return false;
      }
    }
    return true;
  }
  case TypeKind::Enum: {
    return compareTypes(lhs->_enum.repr_type, rhs->_enum.repr_type);
  }
  case TypeKind::Union: {
    for (size_t i = 0; i < lhs->_union.variants.length; i++) {
      if (!compareTypes(lhs->_union.variants.data.ptr[i],
                        rhs->_union.variants.data.ptr[i])) {
        return false;
      }
    }
    return compareTypes(lhs->_union.repr_type, rhs->_union.repr_type);
  }
  }

  return false;
}

/// Checks if `src` can auto convert to `dst`
/// Returns `dst` if it can convert, otherwise `src`
Type *autoConvert(Evaluator *evaluator, Type *src, Type *dst) {
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

void autoCast(Evaluator *evaluator, Node *src, Type *dst) {
  if (src->value.type->kind == TypeKind::Slice &&
      src->value.type->slice.length > 0 && dst->kind == TypeKind::Slice &&
      dst->slice.length == 0) {
    Node *nodes = (Node *)evaluator->allocator->alloc(sizeof(Node) * 2);
    Node *lhs = nodes + 0;
    Node *rhs = nodes + 1;
    *lhs = *src;

    rhs->kind = NodeKind::Dead;
    rhs->value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});
    rhs->value.has_data = true;
    rhs->value.data.type_value = dst;

    src->kind = NodeKind::Operator;
    src->_operator.opcode = Operator::As;
    src->_operator.lhs = lhs;
    src->_operator.rhs = rhs;
    src->value.type = rhs->value.data.type_value;
    return;
  }

  src->value.type = autoConvert(evaluator, src->value.type, dst);
}
