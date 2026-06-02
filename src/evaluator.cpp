#include "evaluator.hpp"
#include "ast.hpp"
#include "operator.hpp"
#include "print.hpp"
#include "symbol.hpp"
#include "types.hpp"
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <system_error>

#define expect(ok, srcloc, msg)                                                \
  if (!(ok)) {                                                                 \
    std::cerr << srcloc << " " << msg << '\n';                                 \
    node->value.type = nullptr;                                                \
    return;                                                                    \
  }

// Forward Declarations
void evaluate(Evaluator *evaluator, Node *node, Symbol *scope);

Value getBuiltinValue(TypeCache *type_cache, String name) {
  std::string str((const char *)name.ptr, name.len);

  Type *out_type = nullptr;
  if (str.compare("void") == 0) {
    out_type = type_cache->get({.kind = TypeKind::Void});
  } else if (str.compare("bool") == 0) {
    out_type = type_cache->get({.kind = TypeKind::Bool});
  } else if (str.compare("usize") == 0) {
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = false, .is_signed = false, .bits = -1};
    out_type = type_cache->get(t);
  } else if (str.compare("isize") == 0) {
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = false, .is_signed = true, .bits = -1};
    out_type = type_cache->get(t);
  } else if (name.len >= 2 &&
             (name[0] == 'u' || name[0] == 'i' || name[0] == 'f')) {
    // Integer and Float
    uint32_t bits = 0;
    auto [ptr, ec] = std::from_chars((const char *)(name.ptr + 1),
                                     (const char *)(name.ptr + name.len), bits);

    if (ec == std::errc{}) {
      Type t = {.kind = TypeKind::Void};
      if (name.ptr[0] == 'u' || name.ptr[0] == 'i') {
        t.kind = TypeKind::Integer;
        t.integer = {
            .is_untyped = false,
            .is_signed = name.ptr[0] == 'i',
            .bits = (int32_t)bits,
        };
      } else if (name.ptr[0] == 'f' &&
                 (bits == 16 || bits == 32 || bits == 64 || bits == 128)) {
        t.kind = TypeKind::Float;
        t._float = {.is_untyped = false, .bits = bits};
      }

      if (t.kind != TypeKind::Void) {
        out_type = type_cache->get(t);
      }
    }
  }

  Value value = {.type = nullptr, .has_data = false};
  if (out_type != nullptr) {
    value.type = type_cache->get({.kind = TypeKind::TypeId});
    value.has_data = true;
    value.data.type_value = out_type;
  } else if (str.compare("true") == 0) {
    value.type = type_cache->get({.kind = TypeKind::Bool});
    value.has_data = true;
    value.data._bool = true;
  } else if (str.compare("false") == 0) {
    value.type = type_cache->get({.kind = TypeKind::Bool});
    value.has_data = true;
    value.data._bool = false;
  }

  return value;
}

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
  if (src->kind != dst->kind) {
    return src;
  }

  if (src->kind == TypeKind::Integer && src->integer.is_untyped) {
    if (dst->integer.is_signed || !src->integer.is_signed) {
      return dst;
    }
  } else if (src->kind == TypeKind::Float && src->_float.is_untyped) {
    return dst;
  }

  return src;
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

    Symbol *access_scope = nullptr;
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
    case TypeKind::Namespace: {
      access_scope = lhs_type->_namespace.scope;
      break;
    }
    default: {
      expect(false, lhs->location,
             "LHS must be a Function, Struct, Enum, Union, or Namespace. Got `"
                 << *lhs_type << "`");
    }
    }

    evaluate(evaluator, node->_operator.rhs, access_scope);
    node->value = node->_operator.rhs->value;
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

    if (lhs_untyped && rhs_untyped) {
      node->value = execute(evaluator, node, scope);
      return;
    } else if (lhs_untyped) {
      lhs->value.type = rhs->value.type;
    } else if (rhs_untyped) {
      rhs->value.type = lhs->value.type;
    }
  }

  // Assign
  if (node->_operator.opcode == Operator::Assign) {
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
      expect(compareTypes(lhs->value.type, rhs->value.type), rhs->location,
             "cannot assign RHS `" << *rhs->value.type << "` to LHS `"
                                   << *lhs->value.type << "`");
    }

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

void evaluate(Evaluator *evaluator, Node *node, Symbol *scope) {
  if (node->value.type != nullptr) {
    return;
  }

  switch (node->kind) {
  case NodeKind::Compound: {
    Symbol *compound_scope = scope->findSymbolByNode(node);
    if (compound_scope == nullptr) {
      compound_scope = scope;
    }

    for (size_t i = 0; i < node->children.length; i++) {
      evaluate(evaluator, node->children.data.ptr[i], compound_scope);
    }
    break;
  }
  case NodeKind::Name: {
    Value builtin_value = getBuiltinValue(evaluator->type_cache, node->text);
    if (builtin_value.type != nullptr) {
      node->value = builtin_value;
      if (node->value.type->kind == TypeKind::Bool) {
        node->kind = NodeKind::Bool;
      }
    } else {
      Symbol *symbol = scope->findSymbol(&node->text, &node->location);
      expect(symbol != nullptr, node->location,
             "Symbol not found: \"" << node->text << '\"');
      evaluate(evaluator, symbol->node, symbol->parent);
      node->value = symbol->node->value;
    }
    break;
  }
  case NodeKind::Integer: {
    Type t;
    t.kind = TypeKind::Integer;
    t.integer = {
        .is_untyped = true,
        .is_signed = node->integer < 0,
        .bits = 0,
    };

    node->value.type = evaluator->type_cache->get(t);
    node->value.has_data = true;
    node->value.data = {.integer = node->integer};
    break;
  }
  case NodeKind::Float: {
    Type t;
    t.kind = TypeKind::Float;
    t._float = {.is_untyped = true, .bits = 0};

    node->value.type = evaluator->type_cache->get(t);
    node->value.has_data = true;
    node->value.data = {._float = node->_float};
    break;
  }
  case NodeKind::Char: {
    Type t;
    t.kind = TypeKind::Integer;
    t.integer = {.is_untyped = false, .is_signed = false, .bits = 8};

    node->value.type = evaluator->type_cache->get(t);
    node->value.has_data = true;
    node->value.data = {.integer = node->integer};
    break;
  }
  case NodeKind::String: {
    Type int_t = {.kind = TypeKind::Integer};
    int_t.integer = {.is_untyped = false, .is_signed = false, .bits = 8};

    // Parse text
    uint8_t *real_text = (uint8_t *)evaluator->allocator->alloc(node->text.len);
    size_t len = 0;
    bool escape = false;
    for (size_t i = 0; i < node->text.len; i++) {
      uint8_t c = node->text.ptr[i];
      if (escape) {
        if (c == '0') {
          c = '\0';
        } else if (c == 'n') {
          c = '\n';
        }
        escape = false;
      } else if (c == '\\') {
        escape = true;
        continue;
      }

      real_text[len] = c;
      len += 1;
    }

    // Set Value
    Type slice_t = {.kind = TypeKind::Slice};
    slice_t.slice = SliceType{
        .length = (int64_t)len,
        .type = evaluator->type_cache->get(int_t),
    };

    node->value.type = evaluator->type_cache->get(slice_t);
    node->value.has_data = true;
    node->value.data.text = {.len = len, .ptr = real_text};
    break;
  }
  case NodeKind::Field: {
    if (node->field.attributes != nullptr) {
      evaluate(evaluator, node->field.attributes, scope);
    }

    Symbol *field_symbol = scope->findSymbolByNode(node);

    if (node->field.type != nullptr) {
      evaluate(evaluator, node->field.type, field_symbol);
      Value *value = &node->field.type->value;
      expect(value->type != nullptr, node->field.type->location,
             "Failed to evaluate field type");
      expect(value->type->kind == TypeKind::TypeId, node->field.type->location,
             "Field type must be a type");

      node->value.type = value->data.type_value;
    }

    if (node->field.initial != nullptr) {
      evaluate(evaluator, node->field.initial, field_symbol);

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
      } else {
        node->field.initial->value.type = autoConvert(
            evaluator, node->field.initial->value.type, value->type);
        if (!compareTypes(node->value.type, value->type)) {
          std::cerr << node->field.initial->location
                    << " Field initial doesn't match type. ";
          std::cerr << "Field Type: `" << *node->value.type << "` ";
          std::cerr << "Initial Type: `" << *value->type << "`\n";
          node->value.type = nullptr;
          return;
        }
      }

      node->value.has_data = value->has_data;
      node->value.data = value->data;
    }
    break;
  }
  case NodeKind::Function: {
    Symbol *fn_scope = scope->findSymbolByNode(node);
    node->value.has_data = false;

    // Prepare type
    Type *fn_t = evaluator->type_cache->get(
        {.kind = TypeKind::Function, .function = {.scope = fn_scope}});
    node->value.type = fn_t;
    fn_t->function.arguments.init(evaluator->allocator, 4);

    // Evaluate parameters
    for (size_t i = 0; i < node->function.parameters.length; i++) {
      Node *param = node->function.parameters.data.ptr[i];
      evaluate(evaluator, param, fn_scope);
      Value *val = &param->value;
      expect(val->type != nullptr, param->location,
             "Failed to evaluate function parameter");
      fn_t->function.arguments.push(val->type);
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
      fn_t->function.return_type = val->data.type_value;
    } else {
      fn_t->function.return_type =
          evaluator->type_cache->get({.kind = TypeKind::Void});
    }

    // Evaluate Body
    if (node->function.body != nullptr) {
      evaluate(evaluator, node->function.body, fn_scope);
    } else if (!node->function.undefined) {
      node->value.has_data = true;
      node->value.data.type_value = node->value.type;
      node->value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});
    }
    break;
  }
  case NodeKind::Struct: {
    Symbol *struct_scope = scope->findSymbolByNode(node);
    node->value.has_data = true;
    node->value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});

    // Prepare type
    Type *struct_t = evaluator->type_cache->get(
        {.kind = TypeKind::Struct, ._struct = {.scope = struct_scope}});
    node->value.data.type_value = struct_t;
    struct_t->_struct.fields.init(evaluator->allocator, 4);
    struct_t->_struct.scope = struct_scope;

    // Evaluate fields
    for (size_t i = 0; i < node->_struct.fields.length; i++) {
      Node *field = node->_struct.fields.data.ptr[i];
      evaluate(evaluator, field, struct_scope);
      Value *val = &field->value;
      expect(val->type != nullptr, field, "Failed to evaluate struct field");
      struct_t->_struct.fields.push(val->type);
    }

    // Evaluate Children
    for (size_t i = 0; i < node->_struct.body.length; i++) {
      evaluate(evaluator, node->_struct.body.data.ptr[i], struct_scope);
    }
    break;
  }
  case NodeKind::Enum: {
    Symbol *enum_scope = scope->findSymbolByNode(node);
    node->value.has_data = true;
    node->value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});

    // Prepare type
    Type *enum_t = evaluator->type_cache->get(
        {.kind = TypeKind::Enum, ._enum = {.scope = enum_scope}});
    node->value.data.type_value = enum_t;

    // Evaluate repr
    if (node->_enum.repr_type != nullptr) {
      evaluate(evaluator, node->_enum.repr_type, scope);
      Value *repr_val = &node->_enum.repr_type->value;
      expect(repr_val->type != nullptr, node->_enum.repr_type->location,
             "Failed to evaluate enum repr type");
      expect(repr_val->type->kind == TypeKind::TypeId,
             node->_enum.repr_type->location, "Enum repr type must be a type");
      expect(repr_val->data.type_value->kind == TypeKind::Integer,
             node->_enum.repr_type->location,
             "Enum repr type is not an integer");

      enum_t->_enum.repr_type = repr_val->data.type_value;
    } else {
      Type repr_t = {.kind = TypeKind::Integer};
      repr_t.integer = IntegerType{
          .is_untyped = false,
          .is_signed = false,
          .bits = 32,
      };
      enum_t->_enum.repr_type = evaluator->type_cache->get(repr_t);
    }

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
    Symbol *union_scope = scope->findSymbolByNode(node);
    node->value.has_data = true;
    node->value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});

    // Prepare type
    Type *union_t = evaluator->type_cache->get(
        {.kind = TypeKind::Union, ._union = {.scope = union_scope}});
    node->value.data.type_value = union_t;
    union_t->_union.variants.init(evaluator->allocator, 4);

    // Evaluate repr
    if (node->_union.repr_type != nullptr) {
      evaluate(evaluator, node->_union.repr_type, scope);
      Value *repr_val = &node->_union.repr_type->value;
      expect(repr_val->type != nullptr, node->_union.repr_type->location,
             "Failed to evaluate union repr type");
      expect(repr_val->type->kind == TypeKind::TypeId,
             node->_union.repr_type->location,
             "Union repr type is not `TypeId`");

      union_t->_union.repr_type = repr_val->data.type_value;
    } else {
      Type repr_t = {.kind = TypeKind::Integer};
      repr_t.integer = IntegerType{
          .is_untyped = false,
          .is_signed = false,
          .bits = 32,
      };
      union_t->_union.repr_type = evaluator->type_cache->get(repr_t);
    }

    // Evaluate variants
    for (size_t i = 0; i < node->_union.variants.length; i++) {
      Node *variant = node->_union.variants.data.ptr[i];
      evaluate(evaluator, variant, union_scope);
      Value *val = &variant->value;
      expect(val->type != nullptr, variant->location,
             "Failed to evaluate union variant");
      union_t->_union.variants.push(val->type);
    }

    // Evaluate Children
    for (size_t i = 0; i < node->_union.body.length; i++) {
      evaluate(evaluator, node->_union.body.data.ptr[i], union_scope);
    }
    break;
  }
  case NodeKind::Namespace: {
    Symbol *namespace_scope = scope->findSymbolByNode(node);

    // Setup Type
    Type ty = {.kind = TypeKind::Namespace};
    ty._namespace.scope = namespace_scope;

    node->value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});
    node->value.has_data = true;
    node->value.data.type_value = evaluator->type_cache->get(ty);

    // Evaluate Children
    for (size_t i = 0; i < node->children.length; i++) {
      evaluate(evaluator, node->children.data.ptr[i], namespace_scope);
    }
    break;
  }
  case NodeKind::Member: {
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = true};

    node->value.type = evaluator->type_cache->get(t),
    node->value.has_data = false;
    if (node->member.value != nullptr) {
      evaluate(evaluator, node->member.value, scope);
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
    evaluate(evaluator, node->import.node, node->import.scope);

    // Type
    node->value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});
    node->value.has_data = true;

    Type ty = {.kind = TypeKind::Namespace,
               ._namespace = {.scope = node->import.scope}};
    node->value.data.type_value = evaluator->type_cache->get(ty);
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
      evaluate(evaluator, node->slice.length, scope);
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
    Symbol *fn_scope = fn_type->function.scope;
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
          receiver_type->kind != TypeKind::Pointer) {
        expect(compareTypes(expected_type->child, receiver_type),
               receiver->location,
               " Receiver `" << *receiver->value.type
                             << "` cannot auto reference to `" << *expected_type
                             << "`");
      } else {
        expect(compareTypes(expected_type, receiver_type), receiver->location,
               "Receiver `" << *receiver->value.type << "` doesn't match `"
                            << *expected_type << "`");
      }
      initial_idx = 1;
    }

    // Evaluate arguments
    for (size_t i = 0; i < node->call.arguments.length; i++) {
      Node *arg = node->call.arguments.data.ptr[i];
      if (i > fn_type->function.arguments.length - initial_idx) {
        std::cerr << arg->location << " Too many arguments\n";
        node->value.type = nullptr;
        return;
      }

      Type *expected_type =
          fn_type->function.arguments.data.ptr[i + initial_idx];

      evaluate(evaluator, arg, scope);
      arg->value.type = autoConvert(evaluator, arg->value.type, expected_type);

      expect(compareTypes(expected_type, arg->value.type), arg->location,
             "Argument `" << *arg->value.type << "` doesn't match expected `"
                          << *expected_type << "`");
    }

    node->value.type = fn_type->function.return_type;
    node->value.has_data = false;
    break;
  }
  case NodeKind::Index: {
    Node *slice = node->index.slice;
    Node *index = node->index.index;
    evaluate(evaluator, slice, scope);
    evaluate(evaluator, index, scope);

    Type *usize_ty = evaluator->type_cache->get(
        {.kind = TypeKind::Integer,
         .integer = {.is_untyped = false, .is_signed = false, .bits = -1}});
    index->value.type = autoConvert(evaluator, index->value.type, usize_ty);

    expect(index->value.type->kind == TypeKind::Integer &&
               !index->value.type->integer.is_signed &&
               index->value.type->integer.bits == -1,
           index->location, "Index must be of type `usize`");

    node->value.type = slice->value.type->slice.type;
    break;
  }
  case NodeKind::Initializer: {
    Node *record = node->initializer.record;
    evaluate(evaluator, record, scope);

    node->value.has_data = false;

    if (record->value.type->kind == TypeKind::TypeId &&
        record->value.data.type_value->kind == TypeKind::Struct) {
      node->value.type = record->value.data.type_value;
      Symbol *struct_symbol = node->value.type->_struct.scope;
      Node *struct_node = struct_symbol->node;

      for (size_t i = 0; i < node->initializer.setters.length; i++) {
        Node *setter = node->initializer.setters.data.ptr[i];
        evaluate(evaluator, setter, scope);

        Type *field_type;
        if (node->initializer.is_list) {
          field_type = node->value.type->_struct.fields.data.ptr[i];
          setter->value.type =
              autoConvert(evaluator, setter->value.type, field_type);
        } else {
          for (size_t l = 0; l < struct_node->_struct.fields.length; l++) {
            Node *field_node = struct_node->_struct.fields.data.ptr[l];
            if (field_node->field.name.compare(setter->member.name)) {
              field_type = field_node->value.type;
              break;
            }
          }

          setter->member.value->value.type = autoConvert(
              evaluator, setter->member.value->value.type, field_type);
        }

        expect(compareTypes(setter->value.type, field_type), setter->location,
               "Setter value doesn't match field type");
      }
    } else if (record->value.type->kind == TypeKind::Slice ||
               record->value.type->kind == TypeKind::SIMD) {
      node->value.type = record->value.type;
      expect(node->initializer.is_list, node->location,
             "Initializer for " << record->value.type->kind
                                << " must be a list");

      for (size_t i = 0; i < node->initializer.setters.length; i++) {
        Node *setter = node->initializer.setters.data.ptr[i];
        evaluate(evaluator, setter, scope);
        setter->value.type = autoConvert(evaluator, setter->value.type,
                                         record->value.type->slice.type);

        expect(compareTypes(setter->value.type, record->value.type->slice.type),
               setter->location, "Setter value doesn't match element type");
      }
    } else {
      expect(
          false, record->location,
          "Initializer type must be one of Struct, Union, Slice, or SIMD. got `"
              << *record->value.type << "`");
    }

    break;
  }
  case NodeKind::Return: {
    Symbol *fn_scope = scope;
    while (fn_scope != nullptr && fn_scope->node->kind != NodeKind::Function) {
      fn_scope = fn_scope->parent;
    }

    expect(fn_scope != nullptr, node->location,
           "Return must be in a function scope");

    Node *fn_node = fn_scope->node;
    Type *expected_type;

    if (fn_node->function.return_type != nullptr) {
      expected_type = fn_node->function.return_type->value.data.type_value;
    } else {
      expected_type = evaluator->type_cache->get({.kind = TypeKind::Void});
    }

    if (node->child == nullptr) {
      expect(expected_type->kind == TypeKind::Void, node->location,
             "Function expects return value");
    } else {
      evaluate(evaluator, node->child, scope);
      node->child->value.type =
          autoConvert(evaluator, node->child->value.type, expected_type);

      expect(compareTypes(expected_type, node->child->value.type),
             node->child->location,
             "Unexpected return value. Got `"
                 << *node->child->value.type << "` Expected `" << *expected_type
                 << "`");
    }

    node->value.type = evaluator->type_cache->get({.kind = TypeKind::Void});
    break;
  }
  case NodeKind::If: {
    Symbol *if_scope = scope->findSymbolByNode(node);

    evaluate(evaluator, node->_if.conditional, if_scope);
    expect(node->_if.conditional->value.type->kind == TypeKind::Bool,
           node->_if.conditional->location, "Conditional must be Bool");

    evaluate(evaluator, node->_if.body, if_scope);

    if (node->_if._else != nullptr) {
      Symbol *else_scope = scope->findSymbolByNode(node->_if._else);
      evaluate(evaluator, node->_if._else, else_scope);
    }

    node->value.type = evaluator->type_cache->get({.kind = TypeKind::Void});
    break;
  }
  case NodeKind::For: {
    Symbol *for_scope = scope->findSymbolByNode(node);
    evaluate(evaluator, node->_for.conditional, for_scope);
    expect(node->_if.conditional->value.type->kind == TypeKind::Bool,
           node->_if.conditional->location, "Conditional must be Bool");

    evaluate(evaluator, node->_for.body, for_scope);

    node->value.type = evaluator->type_cache->get({.kind = TypeKind::Void});
    break;
  }
  case NodeKind::Switch: {
    evaluate(evaluator, node->_switch.conditional, scope);

    for (size_t i = 0; i < node->_switch.cases.length; i++) {
      Node *_case = node->_switch.cases.data.ptr[i];
      evaluate(evaluator, _case, scope);

      Value *case_value = &_case->_case.constant->value;
      expect(
          compareTypes(case_value->type, node->_switch.conditional->value.type),
          _case->_case.constant->location,
          "Unexpected case constant. Got `"
              << *_case->_case.constant->value.type << "` Expected `"
              << *node->_switch.conditional->value.type << "`");
    }

    node->value.type = evaluator->type_cache->get({.kind = TypeKind::Void});
    break;
  }
  case NodeKind::Case: {
    evaluate(evaluator, node->_case.constant, scope);
    execute(evaluator, node->_case.constant, scope);

    Symbol *case_scope = scope->findSymbolByNode(node->_case.body);
    evaluate(evaluator, node->_case.body, case_scope);
    node->value.type = evaluator->type_cache->get({.kind = TypeKind::Void});
    break;
  }
  case NodeKind::Break:
  case NodeKind::Continue: {
    Symbol *loop_scope = scope;
    while (loop_scope != nullptr && loop_scope->node->kind != NodeKind::For) {
      // TODO: handle named loop
      loop_scope = loop_scope->parent;
    }

    expect(loop_scope != nullptr, node->location,
           "" << node->kind << " must be in a loop");

    node->value.type = evaluator->type_cache->get({.kind = TypeKind::Void});
    break;
  }
  case NodeKind::Defer: {
    evaluate(evaluator, node->child, scope);
    node->value.type = evaluator->type_cache->get({.kind = TypeKind::Void});
    break;
  }
  case NodeKind::Comptime: {
    evaluate(evaluator, node->child, scope);
    node->value = execute(evaluator, node, scope);
    break;
  }
  case NodeKind::Assembly: {
    for (size_t i = 0; i < node->assembly.instructions.length; i++) {
      NodeAssembly::Instruction *inst =
          node->assembly.instructions.data.ptr + i;

      for (size_t a = 0; a < inst->arguments.length; a++) {
        NodeAssembly::Argument *arg = inst->arguments.data.ptr + a;
        if (arg->kind == NodeAssembly::Argument::Register) {
          continue;
        }

        evaluate(evaluator, arg->node, scope);
      }
    }
    break;
  }
  case NodeKind::Attribute: {
    for (size_t i = 0; i < node->children.length; i++) {
      Node *child = node->children.data.ptr[i];
      if (child->member.name.compare("link_name")) {
        expect(child->member.value != nullptr, node->location,
               "`link_name` attribute expects value");
        expect(child->member.value->kind == NodeKind::String, node->location,
               "`link_name` attribute expects string value");
      } else if (child->member.name.compare("builtin")) {
        expect(child->member.value == nullptr, node->location,
               "`builtin` attribute expects no value");
      }
    }
    break;
  }
  }
}

void Evaluator::eval() {
  this->type_cache->init(this->allocator);
  this->type_mapping.init(this->allocator, 64);
  this->stack.init(this->allocator, 16);

  evaluate(this, this->ast, this->symbol);
}
