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
    return Value{.type = nullptr};                                             \
  }

Value execute(Evaluator *evaluator, Node *node, Scope *scope) {
  std::cerr << "TODO: Execute compile-time\n";
  return Value{.type = nullptr};
}

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
    expect(symbol != nullptr, node->location,
           "Symbol not found: \"" << node->text << '\"');
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
      expect(value.type != nullptr, node->field.type->location,
             "Failed to evaluate field type");
      expect(value.type->kind == TypeKind::TypeId, node->field.type->location,
             "Field type must be a type");

      out_value.type = value.data.type_value;
    }

    if (node->field.initial != nullptr) {
      Value value = evaluate(evaluator, node->field.initial, scope);
      expect(value.type != nullptr, node->field.initial->location,
             "Failed to evaluate field initial");
      if (out_value.type == nullptr) {
        out_value.type = value.type;
      } else if (out_value.type != value.type) {
        std::cerr << node->field.initial->location
                  << " Field initial doesn't match type.\n";
        std::cerr << "Field Type: " << out_value.type;
        std::cerr << "Initial Type: " << value.type;
        return Value{.type = nullptr};
      }

      out_value.data = value.data;
    }

    return out_value;
  }
  case NodeKind::Function: {
    Value out_value = {.type = nullptr};
    out_value.has_value = true;

    // Prepare type
    Type fn_t = {.kind = TypeKind::Function};
    fn_t.function.arguments.init(evaluator->allocator, 4);

    // Evaluate parameters
    for (size_t i = 0; i < node->function.parameters.length; i++) {
      Node *param = node->function.parameters.data.ptr[i];
      Value val = evaluate(evaluator, param, scope);
      expect(val.type != nullptr, param->location,
             "Failed to evaluate function parameter");
      fn_t.function.arguments.push(val.type);
    }

    // Evaluate return type
    if (node->function.return_type != nullptr) {
      Value val = evaluate(evaluator, node->function.return_type, scope);
      expect(val.type != nullptr, node->function.return_type->location,
             "Failed to evaluate function return type");
      expect(val.type->kind == TypeKind::TypeId,
             node->function.return_type->location,
             "Function return type must be a type");
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
  case NodeKind::Struct: {
    Scope *struct_scope = scope->findScope(node);
    Value out_value;
    out_value.has_value = true;
    out_value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});

    // Prepare type
    Type struct_t = {.kind = TypeKind::Struct};
    struct_t._struct.fields.init(evaluator->allocator, 4);

    // Evaluate fields
    for (size_t i = 0; i < node->_struct.fields.length; i++) {
      Node *field = node->_struct.fields.data.ptr[i];
      Value val = evaluate(evaluator, field, struct_scope);
      expect(val.type != nullptr, field, "Failed to evaluate struct field");
      struct_t._struct.fields.push(val.type);
    }

    // Get type
    out_value.data.type_value = evaluator->type_cache->get(struct_t);

    // Evaluate Children
    for (size_t i = 0; i < node->_struct.body.length; i++) {
      evaluate(evaluator, node->_struct.body.data.ptr[i], struct_scope);
    }

    return out_value;
  }
  case NodeKind::Enum: {
    Value out_value;
    out_value.has_value = true;
    out_value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});

    // Prepare type
    Type enum_t = {.kind = TypeKind::Enum};

    // Evaluate repr
    if (node->_enum.repr_type != nullptr) {
      Value repr_val = evaluate(evaluator, node->_enum.repr_type, scope);
      expect(repr_val.type != nullptr, node->_enum.repr_type->location,
             "Failed to evaluate enum repr type");
      expect(repr_val.type->kind == TypeKind::TypeId,
             node->_enum.repr_type->location, "Enum repr type must be a type");
      expect(repr_val.type->child->kind == TypeKind::Integer,
             node->_enum.repr_type->location,
             "Enum repr type is not an integer");

      enum_t._enum.repr_type = repr_val.data.type_value;
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

    return out_value;
  }
  case NodeKind::Union: {
    Value out_value;
    out_value.has_value = true;
    out_value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});

    // Prepare type
    Type union_t = {.kind = TypeKind::Union};
    union_t._union.variants.init(evaluator->allocator, 4);

    // Evaluate repr
    if (node->_union.repr_type != nullptr) {
      Value repr_val = evaluate(evaluator, node->_union.repr_type, scope);
      expect(repr_val.type != nullptr, node->_union.repr_type->location,
             "Failed to evaluate union repr type");
      expect(repr_val.type->kind == TypeKind::TypeId,
             node->_union.repr_type->location,
             "Union repr type is not `TypeId`");

      union_t._union.repr_type = repr_val.data.type_value;
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
      Value val = evaluate(evaluator, variant, union_scope);
      expect(val.type != nullptr, variant->location,
             "Failed to evaluate union variant");
      union_t._union.variants.push(val.type);
    }

    // Get type
    out_value.data.type_value = evaluator->type_cache->get(union_t);

    // Evaluate Children
    for (size_t i = 0; i < node->_union.body.length; i++) {
      evaluate(evaluator, node->_union.body.data.ptr[i], union_scope);
    }

    return out_value;
  }
  case NodeKind::Member: {
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = true};

    Value out_value = {
        .type = evaluator->type_cache->get(t),
        .has_value = false,
    };
    if (node->member.value != nullptr) {
      Value val = execute(evaluator, node->member.value, scope);
      expect(val.type != nullptr, node->member.value->location,
             "Failed to get member constant");
      expect(val.has_value, node->member.value->location,
             "Member value must be compile-time known");
      expect(val.type->kind == TypeKind::Integer, node->member.value->location,
             "Member value must be an integer");

      out_value.has_value = true;
      out_value.data.integer = val.data.integer;
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
