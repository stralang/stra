#include "evaluator.hpp"
#include "ast.hpp"
#include "print.hpp"
#include "symbol.hpp"
#include "types.hpp"
#include <cstdint>
#include <iostream>

Value evaluate(Evaluator *evaluator, Node *node, Scope *scope) {
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
    if (symbol == nullptr) {
      std::cerr << node->location << " Reference Error. Symbol not found: \""
                << node->text << "\"\n";
      return Value{
          .type = evaluator->type_cache->get(Type{.kind = TypeKind::Void}),
      };
    }

    return evaluate(evaluator, symbol->node, symbol->parent);
  }
  case NodeKind::Integer: {
    Type t;
    t.kind = TypeKind::Integer;
    t.integer.is_untyped = true;

    Type *real_type = evaluator->type_cache->get(t);
    return Value{
        .type = real_type,
        .data = {.integer = node->integer},
    };
  }
  case NodeKind::Float: {
    Type t;
    t.kind = TypeKind::Float;
    t._float.is_untyped = true;

    Type *real_type = evaluator->type_cache->get(t);
    return Value{
        .type = real_type,
        .data = {._float = node->_float},
    };
  }
  case NodeKind::Char: {
    Type t;
    t.kind = TypeKind::Integer;
    t.integer.bits = 8;
    t.integer.is_signed = false;

    Type *real_type = evaluator->type_cache->get(t);
    return Value{
        .type = real_type,
        .data = {.integer = node->integer},
    };
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

    Type *real_type = evaluator->type_cache->get(slice_t);
    return Value{
        .type = real_type,
        .data = {.text = node->text},
    };
  }
  case NodeKind::Field: {
    Value out_value = {.type = nullptr};
    if (node->field.type != nullptr) {
      Value value = evaluate(evaluator, node->field.type, scope);
      if (value.type->kind != TypeKind::TypeId) {
        std::cerr << "Field type must be typeid, got " << value.type->kind
                  << "\n";
        return Value{evaluator->type_cache->get({.kind = TypeKind::Void})};
      }

      out_value.type = value.data.type_value;
    }

    if (node->field.initial != nullptr) {
      Value value = evaluate(evaluator, node->field.initial, scope);
      if (out_value.type == nullptr) {
        out_value.type = value.type;
      } else if (out_value.type != value.type) {
        std::cerr << "Field initial doesn't match type.\n";
        std::cerr << "Field Type: " << out_value.type;
        std::cerr << "Initial Type: " << value.type;
        return Value{evaluator->type_cache->get({.kind = TypeKind::Void})};
      }

      out_value.data = value.data;
    }

    return out_value;
  }
  case NodeKind::Function: {
    Value out_value = {.type = nullptr};

    // Prepare type
    Type fn_t = {.kind = TypeKind::Function};
    fn_t.function.arguments.init(evaluator->allocator, 4);

    // Evaluate parameters
    for (size_t i = 0; i < node->function.parameters.length; i++) {
      Value val =
          evaluate(evaluator, node->function.parameters.data.ptr[i], scope);
      fn_t.function.arguments.push(val.type);
    }

    // Evaluate return type
    if (node->function.return_type != nullptr) {
      Value val = evaluate(evaluator, node->function.return_type, scope);
      if (val.type->kind != TypeKind::TypeId) {
        std::cerr << "Function return type is not `TypeId`\n";
        return Value{evaluator->type_cache->get({.kind = TypeKind::Void})};
      }
      fn_t.function.return_type = val.data.type_value;
    } else {
      fn_t.function.return_type =
          evaluator->type_cache->get({.kind = TypeKind::Void});
    }

    // Get type
    out_value.type = evaluator->type_cache->get(fn_t);

    // Evaluate Body
    Scope *fn_scope = scope->findScope(node);
    if (node->function.body != nullptr) {
      evaluate(evaluator, node->function.body, fn_scope);
    } else if (!node->function.undefined) {
      out_value.data.type_value = out_value.type;
      out_value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});
    }
    return out_value;
  }
  }

  return Value{
      .type = evaluator->type_cache->get(Type{.kind = TypeKind::Void}),
  };
}

void Evaluator::eval() {
  this->type_cache->init(this->allocator);
  this->type_mapping.init(this->allocator, 64);
  this->stack.init(this->allocator, 16);

  evaluate(this, this->ast, this->scope);
}
