#include "../comptime/comptime.hpp"
#include "../print.hpp"
#include "define.hpp"

void evaluateAssignment(Evaluator *evaluator, Node *node, Symbol *scope) {
  Node *lhs = node->_operator.lhs;
  Node *rhs = node->_operator.rhs;
  evaluate(evaluator, lhs, scope);
  evaluate(evaluator, rhs, scope);
  fixUntyped(evaluator, rhs, lhs->value.type);

  if (node->_operator.opcode != Operator::Assign) {
    desugarModifyAssign(evaluator, node, scope);
  }

  // Assign
  expect(!lhs->value.type->is_constant, lhs->location,
         "Cannot assign to constant");

  if (lhs->value.type->kind == TypeKind::Union) {
    bool contains = false;
    for (size_t i = 0; i < lhs->value.type->_union.variants.length; i++) {
      if (compareTypes(lhs->value.type->_union.variants.data.ptr[i],
                       rhs->value.type)) {
        contains = true;
        break;
      }
    }

    expect(contains, rhs->location,
           "Union doesn't contain the type matching RHS `" << *rhs->value.type
                                                           << "`");
  } else if (lhs->kind == NodeKind::Operator &&
             lhs->_operator.lhs->value.type->kind == TypeKind::Union) {
    expect(false, rhs->location,
           "Cannot assign to union member. assign directly to the union "
           "instead");
  } else {
    Type *conv_rhs_type =
        autoConvert(evaluator, rhs->value.type, lhs->value.type);
    expect(compareTypes(lhs->value.type, conv_rhs_type), rhs->location,
           "cannot assign RHS `" << *conv_rhs_type << "` to LHS `"
                                 << *lhs->value.type << "`");
  }

  node->value.type = evaluator->type_cache->get({.kind = TypeKind::Void});
  node->value.has_data = false;
}

void evaluateUnary(Evaluator *evaluator, Node *node, Symbol *scope) {
  evaluate(evaluator, node->unary_operator.child, scope);

  Type *child_type = node->unary_operator.child->value.type;
  Type *child_primitive = child_type;
  if (child_primitive->kind == TypeKind::SIMD) {
    child_primitive = child_primitive->child;
  }

  switch (node->unary_operator.opcode) {
  case UnaryOperator::Minus: {
    if (child_primitive->kind != TypeKind::Integer &&
        child_primitive->kind != TypeKind::Float) {
      expect(false, node->unary_operator.child->location,
             "Child must be of Integer, Float, or SIMD. Got `" << *child_type
                                                               << "`");
    }

    Type out_type = *child_type;
    if (child_primitive->kind == TypeKind::Integer &&
        !child_primitive->integer.is_signed) {
      if (out_type.kind == TypeKind::SIMD) {
        Type prim_out_type = *out_type.child;
        prim_out_type.integer.is_signed = true;
        out_type.child = evaluator->type_cache->get(prim_out_type);
      } else {
        out_type.integer.is_signed = true;
      }
    }

    out_type.is_constant = true;
    node->value.type = evaluator->type_cache->get(out_type);
    node->value.has_data = false;
    break;
  }
  case UnaryOperator::Logical_Not: {
    expect(child_primitive->kind == TypeKind::Bool,
           node->unary_operator.child->location,
           "Child must be Bool, or SIMD. Got `" << *child_type << "`");

    Type out_type = *child_type;
    out_type.is_constant = true;

    node->value.type = evaluator->type_cache->get(out_type);
    node->value.has_data = false;
    break;
  }
  case UnaryOperator::Bitwise_Not: {
    expect(child_primitive->kind == TypeKind::Integer,
           node->unary_operator.child->location,
           "Child must be Integer, or SIMD. Got `" << *child_type << "`");

    Type out_type = *child_type;
    out_type.is_constant = true;

    node->value.type = evaluator->type_cache->get(out_type);
    node->value.has_data = false;
    break;
  }
  case UnaryOperator::Reference: {
    Type out_type = {
        .kind = TypeKind::Pointer, .child = child_type, .is_constant = true};

    node->value.has_data =
        node->unary_operator.child->value.type->kind == TypeKind::TypeId;
    if (node->value.has_data) {
      out_type.child = node->unary_operator.child->value.data.type_value;
      node->value.data.type_value = evaluator->type_cache->get(out_type);
      node->value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});
    } else {
      node->value.type = evaluator->type_cache->get(out_type);
    }
    break;
  }
  case UnaryOperator::Dereference: {
    expect(child_type->kind == TypeKind::Pointer,
           node->unary_operator.child->location,
           "Child must be Pointer. Got `" << *child_type << "`");

    node->value.type = child_type->child;
    node->value.has_data = false;
    break;
  }
  }
}

void evaluateBinary(Evaluator *evaluator, Node *node, Symbol *scope) {
  evaluate(evaluator, node->_operator.lhs, scope);
  Node *lhs = node->_operator.lhs;
  if (node->_operator.opcode == Operator::MemberAccess) {
    Type *lhs_type = lhs->value.type;
    if (lhs_type->kind == TypeKind::TypeId) {
      lhs_type = lhs->value.data.type_value;
    } else if (lhs_type->kind == TypeKind::Pointer) {
      lhs_type = lhs_type->child; // Auto Dereference
    }

    // Slice
    if (lhs_type->kind == TypeKind::Slice) {
      expect(lhs_type->slice.length >= 0, lhs->location,
             "Cannot member access a pointer slice");

      expect(node->_operator.rhs->kind == NodeKind::Name,
             node->_operator.rhs->location,
             "Slice member access must be a name");

      Type out_ty = {};
      if (node->_operator.rhs->text.compare("ptr")) {
        out_ty.kind = TypeKind::Pointer;
        out_ty.is_constant = true;
        out_ty.child = lhs_type->slice.type;
        node->value.has_data = false;
      } else if (node->_operator.rhs->text.compare("len")) {
        out_ty.kind = TypeKind::Integer;
        out_ty.is_constant = true;
        out_ty.integer = {.is_untyped = false, .is_signed = false, .bits = -1};

        node->value.has_data = lhs_type->slice.length > 0;
        node->value.data.integer = lhs_type->slice.length;
      } else {
        expect(false, node->_operator.rhs->location,
               "Unknown slice access must be one of `ptr`, `len`");
      }

      node->value.type = evaluator->type_cache->get(out_ty);
      return;
    }

    // Record
    Symbol *access_scope = nullptr;
    switch (lhs_type->kind) {
    case TypeKind::Struct: {
      access_scope = lhs_type->_struct.scope;
      break;
    }
    case TypeKind::Enum: {
      access_scope = lhs_type->_enum.scope;
      break;
    }
    case TypeKind::Union: {
      access_scope = lhs_type->_union.scope;
      break;
    }
    case TypeKind::Namespace: {
      access_scope = lhs_type->_namespace.scope;
      break;
    }
    default: {
      expect(false, lhs->location,
             "LHS must be a Slice, Struct, Enum, Union, or Namespace. Got `"
                 << *lhs_type << "`");
    }
    }

    evaluate(evaluator, node->_operator.rhs, access_scope);
    node->value = node->_operator.rhs->value;

    // Inject compile-time known result
    if (node->value.has_data) {
      node->kind = NodeKind::Value;
    }
    return;
  }

  evaluate(evaluator, node->_operator.rhs, scope);
  Node *rhs = node->_operator.rhs;

  // Convert from untyped
  if (lhs->value.type->kind == rhs->value.type->kind) {
    bool lhs_untyped = false;
    bool rhs_untyped = false;
    if (lhs->value.type->kind == TypeKind::Integer) {
      lhs_untyped = lhs->value.type->integer.is_untyped;
    } else if (lhs->value.type->kind == TypeKind::Float) {
      lhs_untyped = lhs->value.type->_float.is_untyped;
    }
    if (rhs->value.type->kind == TypeKind::Integer) {
      rhs_untyped = rhs->value.type->integer.is_untyped;
    } else if (rhs->value.type->kind == TypeKind::Float) {
      rhs_untyped = rhs->value.type->_float.is_untyped;
    }

    if (lhs_untyped && !rhs_untyped) {
      fixUntyped(evaluator, lhs, rhs->value.type);
    } else if (!lhs_untyped && rhs_untyped) {
      fixUntyped(evaluator, rhs, lhs->value.type);
    }
  }

  // Get primitive type
  Type *lhs_primitive = lhs->value.type;
  Type *rhs_primitive = rhs->value.type;
  if (lhs_primitive->kind == TypeKind::SIMD) {
    lhs_primitive = lhs_primitive->child;
  }
  if (rhs_primitive->kind == TypeKind::SIMD) {
    rhs_primitive = rhs_primitive->child;
  }

  switch (node->_operator.opcode) {
  case Operator::Add:
  case Operator::Sub:
  case Operator::Mul:
  case Operator::Div:
  case Operator::Mod: {
    if (lhs_primitive->kind != TypeKind::Integer &&
        lhs_primitive->kind != TypeKind::Float &&
        lhs_primitive->kind != TypeKind::Pointer) {
      expect(false, lhs->location,
             "LHS must be of Integer, Float, Pointer, or SIMD. Got `"
                 << *lhs->value.type << "`");
    }

    expect(compareTypes(lhs_primitive, rhs_primitive), rhs->location,
           "LHS `" << *lhs->value.type << "` cannot operate with RHS `"
                   << *rhs->value.type << "`");

    Type out_type = *lhs->value.type;
    out_type.is_constant = true;

    node->value.type = evaluator->type_cache->get(out_type);
    node->value.has_data = false;
    break;
  }
  case Operator::Bitwise_Or:
  case Operator::Bitwise_Xor:
  case Operator::Bitwise_And:
  case Operator::Bitwise_LeftShift:
  case Operator::Bitwise_RightShift: {
    expect(lhs_primitive->kind == TypeKind::Integer, lhs->location,
           "LHS must be an Integer. Got `" << *lhs->value.type << "`");
    expect(compareTypes(lhs_primitive, rhs_primitive), rhs->location,
           "LHS `" << *lhs->value.type << "` cannot operate with RHS `"
                   << *rhs->value.type << "`\n");

    Type out_type = *lhs->value.type;
    out_type.is_constant = true;

    node->value.type = evaluator->type_cache->get(out_type);
    node->value.has_data = false;
    break;
  }
  case Operator::Logical_Or:
  case Operator::Logical_And: {
    expect(lhs_primitive->kind == TypeKind::Bool, lhs->location,
           "LHS must be a Bool. Got `" << *lhs->value.type << "`");
    expect(compareTypes(lhs_primitive, rhs_primitive), rhs->location,
           "LHS `" << *lhs->value.type << "` cannot operate with RHS `"
                   << *rhs->value.type << "`\n");

    Type out_type = *lhs->value.type;
    out_type.is_constant = true;

    node->value.type = evaluator->type_cache->get(out_type);
    node->value.has_data = false;
    break;
  }
  case Operator::EqualTo:
  case Operator::NotEqualTo: {
    expect(compareTypes(lhs->value.type, rhs->value.type), rhs->location,
           "LHS `" << *lhs->value.type << "` cannot operate with RHS `"
                   << *rhs->value.type << "`");

    node->value.type = evaluator->type_cache->get(
        {.kind = TypeKind::Bool, .is_constant = true});
    node->value.has_data = false;
    break;
  }
  case Operator::LessThen:
  case Operator::GreaterThen:
  case Operator::LessThenOrEqualTo:
  case Operator::GreaterThenOrEqualTo: {
    if (lhs_primitive->kind != TypeKind::Integer &&
        lhs_primitive->kind != TypeKind::Float) {
      expect(false, lhs->location,
             "LHS must be of Integer, Float, or SIMD. Got `" << *lhs->value.type
                                                             << "`");
    }

    expect(compareTypes(lhs_primitive, rhs_primitive), rhs->location,
           "LHS `" << *lhs->value.type << "` cannot operate with RHS `"
                   << *rhs->value.type << "`");

    node->value.type = evaluator->type_cache->get(
        {.kind = TypeKind::Bool, .is_constant = true});
    node->value.has_data = false;
    break;
  }
  case Operator::As:
  case Operator::Bitcast: {
    expect(lhs->value.type->kind != TypeKind::TypeId, lhs->location,
           "LHS must not be a type");
    expect(rhs->value.type->kind == TypeKind::TypeId, rhs->location,
           "RHS must be a type");

    node->value.type = rhs->value.data.type_value;
    node->value.has_data = false;

    // `As` cast restrictions
    if (node->_operator.opcode == Operator::Bitcast) {
      break;
    }

    Type *src_type = lhs->value.type;
    Type *dst_type = rhs->value.data.type_value;
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

    expect(allowed, rhs->location,
           "Cannot `as` cast `" << *src_type << "` to `" << *dst_type << "`");
    break;
  }
  case Operator::Assign:
  case Operator::MemberAccess:
  case Operator::Unary_Logical_Not:
  case Operator::Unary_Bitwise_Not: {
    // Already handled
    break;
  }
  }
}
