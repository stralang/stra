#include "evaluator.hpp"
#include "ast.hpp"
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

Value execute(Evaluator *evaluator, Node *node, Scope *scope) {
  std::cerr << "TODO: Execute compile-time\n";
  return Value{.type = nullptr};
}

void evaluate(Evaluator *evaluator, Node *node, Scope *scope) {
  if (node->value.type != nullptr) {
    return;
  }

  switch (node->kind) {
  case NodeKind::Compound: {
    Scope *child_scope = evaluator->scope->findScope(node);
    for (size_t i = 0; i < node->children.length; i++) {
      evaluate(evaluator, node->children.data.ptr[i], child_scope);
    }
    break;
  }
  case NodeKind::Name: {
    Symbol *symbol = evaluator->scope->findSymbol(&node->text, &node->location);
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
        node->value.type = value->type;
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

    Type *type_value = evaluator->type_cache->get(
        {.kind = TypeKind::Constant, .child = val->data.type_value});
    node->value.type = val->type;
    node->value.has_data = true;
    node->value.data.type_value = type_value;
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
  }
}

void Evaluator::eval() {
  this->type_cache->init(this->allocator);
  this->type_mapping.init(this->allocator, 64);
  this->stack.init(this->allocator, 16);

  evaluate(this, this->ast, this->scope);
}
