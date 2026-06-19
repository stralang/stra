#include "evaluator.hpp"
#include "../ast.hpp"
#include "../operator.hpp"
#include "../print.hpp"
#include "../symbol.hpp"
#include "../types.hpp"
#include "define.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

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
    expect(!scope->findDuplicateField(&node->field.name, node), node->location,
           "Field with the name `" << node->field.name << "` already exists");

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
        Type *conv_type = autoConvert(
            evaluator, node->field.initial->value.type, node->value.type);
        if (!compareTypes(node->value.type, conv_type)) {
          std::cerr << node->field.initial->location
                    << " Field initial doesn't match type. ";
          std::cerr << "Field Type: `" << *node->value.type << "` ";
          std::cerr << "Initial Type: `" << *value->type << "`\n";
          evaluator->error_count += 1;
          // node->value.type = nullptr;
          // return;
        }
      }

      node->value.has_data = value->has_data;
      node->value.data = value->data;
    }
    break;
  }
  case NodeKind::Function: {
    evaluateFunction(evaluator, node, scope);
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
      expect(val->type != nullptr, field->location,
             "Failed to evaluate struct field");
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

    expect(!scope->findDuplicateField(&node->member.name, node), node->location,
           "Field with the name `" << node->member.name << "` already exists");

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
      execute(evaluator, node->slice.length, scope);

      expect(node->slice.length->value.type != nullptr,
             node->slice.length->location, "Failed to get slice length");
      expect(node->slice.length->value.type->kind == TypeKind::Integer,
             node->slice.length->location, "Slice length must be an integer");

      t.slice.length = node->slice.length->value.data.integer;
    }

    node->value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});
    node->value.has_data = true;
    node->value.data.type_value = evaluator->type_cache->get(t);
    break;
  }
  case NodeKind::Assignment: {
    evaluateAssignment(evaluator, node, scope);
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
  case NodeKind::Range: {
    evaluate(evaluator, node->range.min, scope);
    evaluate(evaluator, node->range.max, scope);

    expect(node->range.min->value.type == node->range.max->value.type,
           node->location, "Range min and max types must match");

    node->value.type = nullptr;
    node->value.has_data = false;
    break;
  }
  case NodeKind::Call: {
    evaluateCall(evaluator, node, scope);
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
    if (index->kind == NodeKind::Range) {
      // Range
      expect(slice->value.type->kind == TypeKind::Slice ||
                 slice->value.type->kind == TypeKind::SIMD ||
                 slice->value.type->kind == TypeKind::Pointer,
             slice->location,
             "Range Indexee type must be Slice, SIMD, or Pointer");

      Type *min_ty = index->range.min->value.type;
      min_ty = autoConvert(evaluator, min_ty, usize_ty);
      index->range.min->value.type = min_ty;
      index->range.max->value.type = min_ty;

      expect(min_ty->kind == TypeKind::Integer && !min_ty->integer.is_signed &&
                 min_ty->integer.bits == -1,
             index->location, "Range Index must be of type `usize`");

      Type ty = {.kind = TypeKind::Slice};
      ty.slice.length = 0;
      if (slice->value.type->kind == TypeKind::Pointer) {
        ty.slice.type = slice->value.type->child;
      } else {
        ty.slice.type = slice->value.type->slice.type;
      }
      node->value.type = evaluator->type_cache->get(ty);
    } else {
      // Index
      expect(slice->value.type->kind == TypeKind::Slice ||
                 slice->value.type->kind == TypeKind::SIMD,
             slice->location, "Indexee type must be Slice, or SIMD");

      index->value.type = autoConvert(evaluator, index->value.type, usize_ty);

      expect(index->value.type->kind == TypeKind::Integer &&
                 !index->value.type->integer.is_signed &&
                 index->value.type->integer.bits == -1,
             index->location, "Index must be of type `usize`");

      node->value.type = slice->value.type->slice.type;
    }
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
    } else if (record->value.type->kind == TypeKind::TypeId &&
               (record->value.data.type_value->kind == TypeKind::Slice ||
                record->value.data.type_value->kind == TypeKind::SIMD)) {
      node->value.type = record->value.data.type_value;
      expect(node->initializer.is_list, node->location,
             "Initializer for " << record->value.data.type_value->kind
                                << " must be a list");

      for (size_t i = 0; i < node->initializer.setters.length; i++) {
        Node *setter = node->initializer.setters.data.ptr[i];
        evaluate(evaluator, setter, scope);
        setter->value.type =
            autoConvert(evaluator, setter->value.type,
                        record->value.data.type_value->slice.type);

        expect(compareTypes(setter->value.type,
                            record->value.data.type_value->slice.type),
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

    // Desugar
    if (node->_for.conditional->kind == NodeKind::In) {
      Symbol *new_scope = desugarForIn(evaluator, node, for_scope, scope);
      for_scope = new_scope;
      node = new_scope->node;
    }

    // Checks
    evaluate(evaluator, node->_for.conditional, for_scope);
    expect(node->_if.conditional->value.type->kind == TypeKind::Bool,
           node->_if.conditional->location, "Conditional must be Bool");

    evaluate(evaluator, node->_for.body, for_scope);

    node->value.type = evaluator->type_cache->get({.kind = TypeKind::Void});
    break;
  }
  case NodeKind::In: {
    expect(!scope->findDuplicateField(&node->in.name, node), node->location,
           "Field with the name `" << node->in.name << "` already exists");

    evaluate(evaluator, node->in.range, scope);
    expect(node->in.range->range.min->value.type->kind == TypeKind::Integer,
           node->in.range->location,
           "Range must be of integers for `In` expression");

    node->value.type = evaluator->type_cache->get({.kind = TypeKind::Bool});
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
    execute(evaluator, node, scope);
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
