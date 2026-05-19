#include "evaluator.hpp"
#include "ast.hpp"
#include "operator.hpp"
#include "print.hpp"
#include "symbol.hpp"
#include "types.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>

#define expect(ok, srcloc, msg)                                                \
  if (!(ok)) {                                                                 \
    std::cerr << srcloc << " " msg << '\n';                                    \
    node->value.type = nullptr;                                                \
    return;                                                                    \
  }

// Forward Declarations
void evaluate(Evaluator *evaluator, Node *node, Scope *scope);

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
    return lhs->integer.is_signed == rhs->integer.is_signed &&
           (lhs->integer.is_untyped || rhs->integer.is_untyped ||
            lhs->integer.bits == rhs->integer.bits);
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

Value execute(Evaluator *evaluator, Node *node, Scope *scope) {
  std::cerr << "TODO: Execute compile-time\n";
  return Value{.type = nullptr};
}

void evaluateUnary(Evaluator *evaluator, Node *node, Scope *scope) {
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
    node->value.type = evaluator->type_cache->get(out_type);
    node->value.has_data = false;
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

void evaluateBinary(Evaluator *evaluator, Node *node, Scope *scope) {
  evaluate(evaluator, node->_operator.lhs, scope);
  Node *lhs = node->_operator.lhs;
  if (node->_operator.opcode == Operator::MemberAccess) {
    Type *lhs_type = lhs->value.type;
    if (lhs_type->kind == TypeKind::TypeId) {
      lhs_type = lhs->value.data.type_value;
    }

    Scope *access_scope = nullptr;
    switch (lhs_type->kind) {
    case TypeKind::Function: {
      access_scope = lhs_type->function.scope;
      break;
    }
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
    default: {
      expect(false, lhs->location,
             "LHS must be a Function, Struct, Enum, or Union. Got `"
                 << *lhs_type << "`");
    }
    }

    evaluate(evaluator, node->_operator.rhs, access_scope);
    node->value = node->_operator.rhs->value;
    return;
  }

  evaluate(evaluator, node->_operator.rhs, scope);
  Node *rhs = node->_operator.rhs;

  // Assign
  if (node->_operator.opcode == Operator::Assign) {
    expect(!lhs->value.type->is_constant, lhs->location,
           "Cannot assign to constant");

    expect(compareTypes(lhs->value.type, rhs->value.type), rhs->location,
           "cannot assign RHS `" << *rhs->value.type << "` to LHS `"
                                 << *lhs->value.type << "`");

    // TODO: Check Types
    node->value.type = evaluator->type_cache->get({.kind = TypeKind::Void});
    node->value.has_data = false;
    return;
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
                   << *rhs->value.type);

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
    expect(lhs->value.type == rhs->value.type, rhs->location,
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

void evaluate(Evaluator *evaluator, Node *node, Scope *scope) {
  if (node->value.type != nullptr) {
    return;
  }

  switch (node->kind) {
  case NodeKind::Compound: {
    Scope *child_scope = scope->findScope(node);
    for (size_t i = 0; i < node->children.length; i++) {
      evaluate(evaluator, node->children.data.ptr[i], child_scope);
    }
    break;
  }
  case NodeKind::Name: {
    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    expect(symbol != nullptr, node->location,
           "Symbol not found: \"" << node->text << '\"');
    evaluate(evaluator, symbol->node, symbol->parent);
    node->value = symbol->node->value;
    break;
  }
  case NodeKind::Integer: {
    Type t;
    t.kind = TypeKind::Integer;
    t.integer.is_untyped = true;

    node->value.type = evaluator->type_cache->get(t);
    node->value.has_data = true;
    node->value.data = {.integer = node->integer};
    break;
  }
  case NodeKind::Float: {
    Type t;
    t.kind = TypeKind::Float;
    t._float.is_untyped = true;

    node->value.type = evaluator->type_cache->get(t);
    node->value.has_data = true;
    node->value.data = {._float = node->_float};
    break;
  }
  case NodeKind::Char: {
    Type t;
    t.kind = TypeKind::Integer;
    t.integer.bits = 8;
    t.integer.is_signed = false;

    node->value.type = evaluator->type_cache->get(t);
    node->value.has_data = true;
    node->value.data = {.integer = node->integer};
    break;
  }
  case NodeKind::String: {
    Type int_t = {.kind = TypeKind::Integer};
    int_t.integer = IntegerType{
        .is_untyped = false,
        .is_signed = false,
        .bits = 8,
    };

    Type slice_t = {.kind = TypeKind::Slice};
    slice_t.slice = SliceType{
        .length = (int64_t)node->text.len,
        .type = evaluator->type_cache->get(int_t),
    };

    node->value.type = evaluator->type_cache->get(slice_t);
    node->value.has_data = true;
    node->value.data = {.text = node->text};
    break;
  }
  case NodeKind::Field: {
    if (node->field.type != nullptr) {
      evaluate(evaluator, node->field.type, scope);
      Value *value = &node->field.type->value;
      expect(value->type != nullptr, node->field.type->location,
             "Failed to evaluate field type");
      expect(value->type->kind == TypeKind::TypeId, node->field.type->location,
             "Field type must be a type");

      node->value.type = value->data.type_value;
    }

    if (node->field.initial != nullptr) {
      evaluate(evaluator, node->field.initial, scope);
      Value *value = &node->field.initial->value;
      expect(value->type != nullptr, node->field.initial->location,
             "Failed to evaluate field initial");
      if (node->value.type == nullptr) {
        if (value->type->is_constant && !node->field.definition) {
          Type field_type = *value->type;
          field_type.is_constant = false;
          node->value.type = evaluator->type_cache->get(field_type);
        } else {
          node->value.type = value->type;
        }
      } else if (node->value.type != value->type) {
        std::cerr << node->field.initial->location
                  << " Field initial doesn't match type.\n";
        std::cerr << "Field Type: " << node->value.type;
        std::cerr << "Initial Type: " << value->type;
        node->value.type = nullptr;
        return;
      }

      node->value.has_data = true;
      node->value.data = value->data;
    }
    break;
  }
  case NodeKind::Function: {
    node->value.has_data = true;

    // Prepare type
    Type fn_t = {.kind = TypeKind::Function};
    fn_t.function.arguments.init(evaluator->allocator, 4);

    // Evaluate parameters
    for (size_t i = 0; i < node->function.parameters.length; i++) {
      Node *param = node->function.parameters.data.ptr[i];
      evaluate(evaluator, param, scope);
      Value *val = &param->value;
      expect(val->type != nullptr, param->location,
             "Failed to evaluate function parameter");
      fn_t.function.arguments.push(val->type);
    }

    // Evaluate return type
    if (node->function.return_type != nullptr) {
      evaluate(evaluator, node->function.return_type, scope);
      Value *val = &node->function.return_type->value;
      expect(val->type != nullptr, node->function.return_type->location,
             "Failed to evaluate function return type");
      expect(val->type->kind == TypeKind::TypeId,
             node->function.return_type->location,
             "Function return type must be a type");
      fn_t.function.return_type = val->data.type_value;
    } else {
      fn_t.function.return_type =
          evaluator->type_cache->get({.kind = TypeKind::Void});
    }

    // Get type
    node->value.type = evaluator->type_cache->get(fn_t);

    // Evaluate Body
    Scope *fn_scope = scope->findScope(node);
    fn_t.function.scope = fn_scope;
    if (node->function.body != nullptr) {
      evaluate(evaluator, node->function.body, fn_scope);
    } else if (!node->function.undefined) {
      node->value.data.type_value = node->value.type;
      node->value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});
    }
    break;
  }
  case NodeKind::Struct: {
    Scope *struct_scope = scope->findScope(node);
    node->value.has_data = true;
    node->value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});

    // Prepare type
    Type struct_t = {.kind = TypeKind::Struct};
    struct_t._struct.fields.init(evaluator->allocator, 4);
    struct_t._struct.scope = struct_scope;

    // Evaluate fields
    for (size_t i = 0; i < node->_struct.fields.length; i++) {
      Node *field = node->_struct.fields.data.ptr[i];
      evaluate(evaluator, field, struct_scope);
      Value *val = &field->value;
      expect(val->type != nullptr, field, "Failed to evaluate struct field");
      struct_t._struct.fields.push(val->type);
    }

    // Get type
    node->value.data.type_value = evaluator->type_cache->get(struct_t);

    // Evaluate Children
    for (size_t i = 0; i < node->_struct.body.length; i++) {
      evaluate(evaluator, node->_struct.body.data.ptr[i], struct_scope);
    }
    break;
  }
  case NodeKind::Enum: {
    node->value.has_data = true;
    node->value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});

    // Prepare type
    Type enum_t = {.kind = TypeKind::Enum};

    // Evaluate repr
    if (node->_enum.repr_type != nullptr) {
      evaluate(evaluator, node->_enum.repr_type, scope);
      Value *repr_val = &node->_enum.repr_type->value;
      expect(repr_val->type != nullptr, node->_enum.repr_type->location,
             "Failed to evaluate enum repr type");
      expect(repr_val->type->kind == TypeKind::TypeId,
             node->_enum.repr_type->location, "Enum repr type must be a type");
      expect(repr_val->type->child->kind == TypeKind::Integer,
             node->_enum.repr_type->location,
             "Enum repr type is not an integer");

      enum_t._enum.repr_type = repr_val->data.type_value;
    } else {
      Type repr_t = {.kind = TypeKind::Integer};
      repr_t.integer = IntegerType{
          .is_untyped = false,
          .is_signed = false,
          .bits = 32,
      };
      enum_t._enum.repr_type = evaluator->type_cache->get(repr_t);
    }

    // Get scope
    Scope *enum_scope = scope->findScope(node);
    enum_t._enum.scope = enum_scope;

    // Evaluate members
    for (size_t i = 0; i < node->_enum.members.length; i++) {
      evaluate(evaluator, node->_enum.members.data.ptr[i], enum_scope);
    }

    // Evaluate children
    for (size_t i = 0; i < node->_enum.body.length; i++) {
      evaluate(evaluator, node->_enum.body.data.ptr[i], enum_scope);
    }
    break;
  }
  case NodeKind::Union: {
    node->value.has_data = true;
    node->value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});

    // Prepare type
    Type union_t = {.kind = TypeKind::Union};
    union_t._union.variants.init(evaluator->allocator, 4);

    // Evaluate repr
    if (node->_union.repr_type != nullptr) {
      evaluate(evaluator, node->_union.repr_type, scope);
      Value *repr_val = &node->_union.repr_type->value;
      expect(repr_val->type != nullptr, node->_union.repr_type->location,
             "Failed to evaluate union repr type");
      expect(repr_val->type->kind == TypeKind::TypeId,
             node->_union.repr_type->location,
             "Union repr type is not `TypeId`");

      union_t._union.repr_type = repr_val->data.type_value;
    } else {
      Type repr_t = {.kind = TypeKind::Integer};
      repr_t.integer = IntegerType{
          .is_untyped = false,
          .is_signed = false,
          .bits = 32,
      };
      union_t._union.repr_type = evaluator->type_cache->get(repr_t);
    }

    // Get scope
    Scope *union_scope = scope->findScope(node);
    union_t._union.scope = union_scope;

    // Evaluate variants
    for (size_t i = 0; i < node->_union.variants.length; i++) {
      Node *variant = node->_union.variants.data.ptr[i];
      evaluate(evaluator, variant, union_scope);
      Value *val = &variant->value;
      expect(val->type != nullptr, variant->location,
             "Failed to evaluate union variant");
      union_t._union.variants.push(val->type);
    }

    // Get type
    node->value.data.type_value = evaluator->type_cache->get(union_t);

    // Evaluate Children
    for (size_t i = 0; i < node->_union.body.length; i++) {
      evaluate(evaluator, node->_union.body.data.ptr[i], union_scope);
    }
    break;
  }
  case NodeKind::Member: {
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = true};

    node->value.type = evaluator->type_cache->get(t),
    node->value.has_data = false;
    if (node->member.value != nullptr) {
      execute(evaluator, node->member.value, scope);
      Value *val = &node->member.value->value;
      expect(val->type != nullptr, node->member.value->location,
             "Failed to get member constant");
      expect(val->has_data, node->member.value->location,
             "Member value must be compile-time known");
      expect(val->type->kind == TypeKind::Integer, node->member.value->location,
             "Member value must be an integer");

      node->value.has_data = true;
      node->value.data.integer = val->data.integer;
    }
    break;
  }
  case NodeKind::Import: {
    // TODO: Import
    break;
  }
  case NodeKind::Const: {
    evaluate(evaluator, node->child, scope);
    Value *val = &node->child->value;
    expect(val->type != nullptr, node->child->location,
           "Failed to get child type");
    expect(val->type->kind == TypeKind::TypeId, node->child->location,
           "Child type must be a type");

    Type out_type = *val->data.type_value;
    out_type.is_constant = true;
    node->value.type = val->type;
    node->value.has_data = true;
    node->value.data.type_value = evaluator->type_cache->get(out_type);
    break;
  }
  case NodeKind::Slice: {
    evaluate(evaluator, node->slice.type, scope);
    Value *val = &node->slice.type->value;
    expect(val->type != nullptr, node->slice.type->location,
           "Failed to get element type");
    expect(val->type->kind == TypeKind::TypeId, node->slice.type->location,
           "Element type must be a type");

    Type t = {.kind = TypeKind::Slice};
    t.slice.type = val->data.type_value;
    t.slice.length = 0;

    if (node->slice.is_pointer) {
      t.slice.length = -1;
    } else if (node->slice.length != nullptr) {
      Value val = execute(evaluator, node->slice.length, scope);
      expect(val.type != nullptr, node->slice.length->location,
             "Failed to get slice length");
      expect(val.type->kind == TypeKind::Integer, node->slice.length,
             "Slice length must be an integer");

      t.slice.length = val.data.integer;
    }

    node->value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});
    node->value.has_data = true;
    node->value.data.type_value = evaluator->type_cache->get(t);
    break;
  }
  case NodeKind::UnaryOperator: {
    evaluateUnary(evaluator, node, scope);
    break;
  }
  case NodeKind::Operator: {
    evaluateBinary(evaluator, node, scope);
    break;
  }
  case NodeKind::Call: {
    Node *callee = node->call.callee;
    evaluate(evaluator, callee, scope);

    // Get function
    expect(callee->value.type->kind == TypeKind::Function, callee->location,
           "Callee must be a function. Got `" << *callee->value.type << "`");

    Type *fn_type = callee->value.type;
    Scope *fn_scope = fn_type->function.scope;
    Node *method = scope->node;

    // Get receiver
    Node *receiver = nullptr;
    size_t initial_idx = 0;
    if (callee->kind == NodeKind::Operator &&
        callee->_operator.opcode == Operator::MemberAccess &&
        callee->_operator.lhs->value.type->kind != TypeKind::TypeId) {
      receiver = callee->_operator.lhs;
      expect(fn_type->function.arguments.length >= 1, receiver->location,
             "Receiver expects method with atleast 1 argument");

      Type *expected_type = fn_type->function.arguments.data.ptr[0];
      Type *receiver_type = receiver->value.type;
      if (expected_type->kind == TypeKind::Pointer &&
          receiver_type->kind != TypeKind::Pointer &&
          !compareTypes(expected_type, receiver_type->child)) {
        std::cerr << receiver->location << " Receiver `"
                  << *receiver->value.type << "` cannot auto reference to `"
                  << *expected_type << "`\n";
        node->value.type = nullptr;
        return;
      } else {
        expect(compareTypes(expected_type, receiver_type), receiver->location,
               "Receiver `" << *receiver->value.type << "` doesn't match `"
                            << *expected_type << "`");
      }
      initial_idx = 1;
    }

    // Evaluate arguments
    for (size_t i = initial_idx; i < node->call.arguments.length; i++) {
      Node *arg = node->call.arguments.data.ptr[i];
      if (i > fn_type->function.arguments.length) {
        std::cerr << arg->location << " Too many arguments\n";
        node->value.type = nullptr;
        return;
      }

      evaluate(evaluator, arg, fn_scope);

      Type *expected_type = fn_type->function.arguments.data.ptr[i];
      expect(compareTypes(expected_type, arg->value.type), arg->location,
             "Argument `" << *arg->value.type << "` doesn't match expected `"
                          << *expected_type << "`");
    }

    node->value.type = fn_type->function.return_type;
    node->value.has_data = false;
    break;
  }
  }
}

void Evaluator::eval() {
  this->type_cache->init(this->allocator);
  this->type_mapping.init(this->allocator, 64);
  this->stack.init(this->allocator, 16);

  evaluate(this, this->ast, this->scope);
}
