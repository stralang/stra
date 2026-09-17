#include "uirgen.hpp"
#include "../print.hpp"
#include "define.hpp"
#include "uir/literal.hpp"
#include "uir/uir.hpp"
#include <functional>
#include <iostream>

UIRValue *genComptime(UIRGen *uirgen, Node *child, Symbol *scope) {
  UIRValue *value = uirgen->builder.buildComptime(str("comptime"));
  value->comptime.blocks.init(uirgen->allocator, 32);

  UIRBlock *prev_block = uirgen->builder.block;
  uirgen->builder.block = uirgen->builder.appendBlock(value, str("entry"));

  UIRValue *output = gen(uirgen, child, scope);
  if (!uirgen->builder.block->hasTerminator()) {
    uirgen->builder.buildReturn(output);
  }

  uirgen->builder.block = prev_block;
  return value;
}

UIRValue *addr(UIRGen *uirgen, Node *node, Symbol *scope) {
  switch (node->kind) {
  case NodeKind::Name: {
    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    if (symbol == nullptr) {
      expect(false, node->location,
             "Couldn't find symbol `" << node->text << "`");
      return nullptr;
    }

    UIRValue **value = uirgen->node_to_value.get(symbol->node);
    if (value == nullptr) {
      expect(false, node->location,
             " !UIR! Failed to get instruction for symbol `" << node->text
                                                             << "`");
      return nullptr;
    }

    return *value;
  }
  case NodeKind::Operator: {
    if (node->_operator.opcode == Operator::MemberAccess) {
      return addrMemberAccess(uirgen, node, scope);
    }
    break;
  }
  case NodeKind::UnaryOperator: {
    if (node->unary_operator.opcode == UnaryOperator::Dereference) {
      return gen(uirgen, node->unary_operator.child, scope);
    }
  }
  case NodeKind::Index: {
    UIRValue *ptr = addr(uirgen, node->index.slice, scope);

    UIRValue *out = nullptr;
    if (node->index.index->kind == NodeKind::Range) {
      UIRValue *offset = gen(uirgen, node->index.index->range.min, scope);
      UIRValue *length = gen(uirgen, node->index.index->range.max, scope);
      UIRValue *new_slice = uirgen->builder.buildRange(ptr, offset, length);

      UIRValue *ty = uirgen->builder.buildTypeOf(new_slice);
      out = uirgen->builder.buildLocalVariable(ty, str("tmp_uir_intern"));
      uirgen->builder.buildStore(new_slice, out);
    } else {
      UIRValue *index = gen(uirgen, node->index.index, scope);
      out = uirgen->builder.buildIndex(ptr, index);
    }

    out->source_location = node->location;
    return out;
  }
  }

  return nullptr;
}

UIRValue *gen(UIRGen *uirgen, Node *node, Symbol *scope) {
  switch (node->kind) {
  case NodeKind::Compound: {
    for (size_t i = 0; i < node->children.length; i++) {
      gen(uirgen, node->children.getUnchecked(i), scope);
    }
    break;
  }
  case NodeKind::Block: {
    Symbol *block_scope = scope->findSymbolByNode(node);

    // Set Defer
    size_t old_defer_len = uirgen->defer_stack_len;
    size_t old_defer_boundary = uirgen->defer_local_boundary;

    // Blocks
    UIRValue *parent = uirgen->builder.block->parent;
    UIRBlock *body_block = uirgen->builder.appendBlock(parent, str("scope"));
    UIRBlock *merge_block =
        uirgen->builder.appendBlock(parent, str("scope_merge"));

    // Generate Body
    uirgen->builder.buildBr(body_block);
    uirgen->builder.block = body_block;
    for (size_t i = 0; i < node->children.length; i++) {
      gen(uirgen, node->children.getUnchecked(i), block_scope);
    }

    injectDefer(uirgen, block_scope, false);
    uirgen->builder.buildBr(merge_block);
    uirgen->builder.block = merge_block;

    // Reset Defer
    uirgen->defer_stack_len = old_defer_len;
    uirgen->defer_local_boundary = old_defer_boundary;

    break;
  }
  case NodeKind::Name: {
    UIRValue *builtin = genBuiltin(uirgen, node->text);
    if (builtin != nullptr) {
      return builtin;
    }

    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    if (symbol == nullptr) {
      expect(false, node->location,
             " Couldn't find symbol `" << node->text << "`");
      return nullptr;
    }

    UIRValue **value = uirgen->node_to_value.get(symbol->node);
    if (value == nullptr) {
      expect(false, node->location,
             " !UIR! Failed to get instruction for Symbol `" << node->text
                                                             << "`");
      return nullptr;
    }

    UIRValue *out = uirgen->builder.buildLoad(*value, {.ptr = nullptr});
    out->source_location = node->location;
    return out;
  }
  case NodeKind::RawString: {
    Type int_t = {.kind = TypeKind::Integer};
    int_t.integer = {.is_untyped = false, .is_signed = false, .bits = 8};

    // Parse text
    uint8_t *real_text =
        (uint8_t *)uirgen->allocator->allocZeroed(node->text.len);
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
        .type = uirgen->ctx->type_cache->get(int_t),
    };

    UIRLiteral slice_lit = {
        .lit_type = uirgen->ctx->type_cache->get(slice_t),
        .kind = UIRLiteralKind::Typed,
    };
    slice_lit.inline_data = real_text; // Length is stored in the type
    return uirgen->builder.buildLiteral(slice_lit);
  }
  case NodeKind::Value: {
    return valueToUIR(uirgen, &node->value);
  }
  case NodeKind::Field: {
    // Get Symbol
    Symbol *field_symbol = scope->findSymbolByNode(node);

    // TODO: Add attributes

    // Generate field
    UIRValue *field = nullptr;
    if (scope->location_aware) {
      UIRValue *initial = nullptr;
      UIRValue *type = nullptr;
      if (node->field.initial != nullptr) {
        initial = gen(uirgen, node->field.initial, field_symbol);
      }
      if (node->field.type != nullptr) {
        type = genComptime(uirgen, node->field.type, field_symbol);
      } else {
        type = uirgen->builder.buildTypeOf(initial, {.ptr = nullptr});
      }

      field = uirgen->builder.buildLocalVariable(type, node->field.name);
      uirgen->node_to_value.insert(node, field);

      if (initial != nullptr) {
        UIRValue *store_inst = uirgen->builder.buildStore(initial, field);
        store_inst->source_location = node->location;
      }
    } else if (node->field.initial != nullptr &&
               node->field.initial->kind == NodeKind::Function) {
      field = gen(uirgen, node->field.initial, field_symbol);
    } else {
      field = *uirgen->node_to_value.get(node); // Pre-generated

      if (node->field.type != nullptr) {
        field->global_variable.type =
            genComptime(uirgen, node->field.type, field_symbol);
      }
      if (node->field.initial != nullptr) {
        field->global_variable.constant =
            genComptime(uirgen, node->field.initial, field_symbol);
      }
      field->global_variable.undefined = node->field.undefined;
    }

    field->source_location = node->location;
    return field;
  }
  case NodeKind::Function: {
    Symbol *fn_symbol = scope->findSymbolByNode(node);

    // Parameter Types
    Slice<UIRValue *> parameters = {
        .ptr = (UIRValue **)uirgen->allocator->allocZeroed(
            sizeof(UIRValue *) * node->function.parameters.length),
        .len = node->function.parameters.length,
    };

    for (size_t i = 0; i < node->function.parameters.length; i++) {
      Node *arg = node->function.parameters.getUnchecked(i);
      parameters.ptr[i] = genComptime(uirgen, arg->field.type, fn_symbol);
    }

    // Return Type
    UIRValue *return_type = nullptr;
    if (node->function.return_type == nullptr) {
      UIRLiteral literal = {
          .lit_type = uirgen->ctx->type_cache->get({.kind = TypeKind::TypeId}),
          ._typeid = uirgen->ctx->type_cache->get({.kind = TypeKind::Void}),
      };

      return_type = uirgen->builder.buildLiteral(literal);
    } else {
      return_type = genComptime(uirgen, node->function.return_type, fn_symbol);
    }

    // Build Function
    UIRValue *value = nullptr;
    UIRValue **cache = uirgen->node_to_value.get(node);
    if (cache != nullptr) {
      value = *cache; // Pre-generated
      value->function.parameter_types = parameters;
      value->function.return_type = return_type;
    } else {
      value = uirgen->builder.buildFunction(parameters, return_type, str(""));
    }
    value->source_location = node->location;

    // Body
    value->function.undefined = node->function.undefined;
    if (node->function.body != nullptr) {
      uirgen->module->scopes.push({.owner = value});
      UIRScope *globals = uirgen->module->scopes.back();
      globals->list.init(uirgen->builder.module->allocator, 32);
      value->function.globals = globals;

      value->function.blocks.init(uirgen->allocator, 32);
      UIRBlock *entry_block = uirgen->builder.appendBlock(value, str("entry"));

      UIRBlock *prev_block = uirgen->builder.block;
      UIRScope *prev_scope = uirgen->builder.scope;
      uirgen->builder.block = entry_block;
      uirgen->builder.scope = globals;

      // Parameters
      for (size_t i = 0; i < node->function.parameters.length; i++) {
        Node *arg = node->function.parameters.getUnchecked(i);
        UIRValue *uir_arg = uirgen->builder.buildArg(
            value->function.parameter_types.ptr[i], arg->field.name);
        uir_arg->source_location = arg->location;
        uirgen->node_to_value.insert(arg, uir_arg);
      }

      // Body
      gen(uirgen, node->function.body, fn_symbol);

      // Inject void return
      if (!uirgen->builder.block->hasTerminator()) {
        injectDefer(uirgen, fn_symbol, false);
        uirgen->builder.buildReturn(nullptr);
      }

      // End
      uirgen->builder.block = prev_block;
      uirgen->builder.scope = prev_scope;
      uirgen->defer_stack_len = 0; // Clear defer stack
    }

    return value;
  }
  case NodeKind::Struct: {
    return genStruct(uirgen, node, scope);
  }
  case NodeKind::Enum: {
    return genEnum(uirgen, node, scope);
  }
  case NodeKind::Union: {
    return genUnion(uirgen, node, scope);
  }
  case NodeKind::Namespace: {
    return genNamespace(uirgen, node, scope);
  }
  case NodeKind::Slice: {
    UIRValue *element = gen(uirgen, node->slice.type, scope);
    UIRValue *length = nullptr;
    if (node->slice.length != nullptr) {
      length = gen(uirgen, node->slice.length, scope);
    }

    UIRValue *out = uirgen->builder.buildSlice(
        element, length, node->slice.is_pointer, {.ptr = nullptr});
    out->source_location = node->location;
    return out;
  }
  case NodeKind::Assignment: {
    return genAssignment(uirgen, node, scope);
  }
  case NodeKind::UnaryOperator: {
    return genUnary(uirgen, node, scope);
  }
  case NodeKind::Operator: {
    return genBinary(uirgen, node, scope);
  }
  case NodeKind::Call: {
    UIRValue *callee = addr(uirgen, node->call.callee, scope);
    Slice<UIRValue *> arguments = {
        .ptr = (UIRValue **)uirgen->allocator->allocZeroed(
            sizeof(UIRValue *) * node->call.arguments.length),
        .len = node->call.arguments.length,
    };

    // Receiver
    UIRValue *receiver = nullptr;
    if (node->call.callee->_operator.opcode == Operator::MemberAccess) {
      receiver = addr(uirgen, node->call.callee->_operator.lhs, scope);
    }

    // Arguments
    for (size_t i = 0; i < arguments.len; i++) {
      arguments.ptr[i] =
          gen(uirgen, node->call.arguments.getUnchecked(i), scope);
    }

    UIRValue *out = uirgen->builder.buildCall(callee, arguments, receiver,
                                              {.ptr = nullptr});
    out->source_location = node->location;
    return out;
  }
  case NodeKind::Index: {
    if (node->index.index->kind == NodeKind::Range) {
      UIRValue *ptr = addr(uirgen, node->index.slice, scope);
      UIRValue *offset = gen(uirgen, node->index.index->range.min, scope);
      UIRValue *length = gen(uirgen, node->index.index->range.max, scope);

      UIRValue *out = uirgen->builder.buildRange(ptr, offset, length);
      out->source_location = node->location;
      return out;
    }
    UIRValue *ptr = addr(uirgen, node, scope);
    return uirgen->builder.buildLoad(ptr);
  }
  case NodeKind::Initializer: {
    UIRValue *record_type =
        genComptime(uirgen, node->initializer.record, scope);
    Slice<UIRValue *> values = {
        .ptr = (UIRValue **)uirgen->allocator->allocZeroed(
            sizeof(UIRValue *) * node->initializer.setters.length),
        .len = node->initializer.setters.length,
    };
    Slice<String> names = {.ptr = nullptr, .len = 0};

    // Prepare Named
    if (!node->initializer.is_list) {
      names = {
          .ptr = (String *)uirgen->allocator->allocZeroed(sizeof(String) *
                                                          values.len),
          .len = values.len,
      };
    }

    // Get names and values
    for (size_t i = 0; i < node->initializer.setters.length; i++) {
      Node *setter = node->initializer.setters.getUnchecked(i);
      if (names.ptr != nullptr) {
        names.ptr[i] = setter->member.name;
        values.ptr[i] = gen(uirgen, setter->member.value, scope);
      } else {
        values.ptr[i] = gen(uirgen, setter, scope);
      }
    }

    // Create instruction
    UIRValue *out = uirgen->builder.buildAggregate(record_type, names, values);
    out->source_location = node->location;
    return out;
  }
  case NodeKind::Return: {
    injectDefer(uirgen, scope, true);

    // Generate value
    UIRValue *ret_value = nullptr;
    if (node->child != nullptr) {
      ret_value = gen(uirgen, node->child, scope);
    }

    // Build
    UIRValue *out = uirgen->builder.buildReturn(ret_value);
    out->source_location = node->location;
    return out;
  }
  case NodeKind::If: {
    genIf(uirgen, node, scope);
    break;
  }
  case NodeKind::For: {
    genLoop(uirgen, node, scope);
    break;
  }
  case NodeKind::Switch: {
    genSwitch(uirgen, node, scope);
    break;
  }
  case NodeKind::Defer: {
    uirgen->defer_stack[uirgen->defer_stack_len] = node->child;
    uirgen->defer_stack_len += 1;
    break;
  }
  case NodeKind::Comptime: {
    return genComptime(uirgen, node->child, scope);
  }
  }

  return nullptr;
}

void injectDefer(UIRGen *uirgen, Symbol *scope, bool is_return) {
  size_t i = uirgen->defer_stack_len;
  size_t min = is_return ? 0 : uirgen->defer_local_boundary;

  while (i > min) {
    i -= 1;
    gen(uirgen, uirgen->defer_stack[i], scope);
  }
}

void genDeclaration(UIRGen *uirgen, Node *node, Symbol *scope) {
  switch (node->kind) {
  case NodeKind::Compound: {
    for (size_t i = 0; i < node->children.length; i++) {
      genDeclaration(uirgen, node->children.getUnchecked(i), scope);
    }
    break;
  }
  case NodeKind::Field: {
    UIRValue *field = nullptr;
    if (node->field.initial != nullptr &&
        node->field.initial->kind == NodeKind::Function) {
      field = uirgen->builder.buildFunction({.ptr = nullptr}, nullptr,
                                            node->field.name);
      uirgen->node_to_value.insert(node->field.initial, field);
    } else {
      field = uirgen->builder.buildGlobalVariable(nullptr, nullptr,
                                                  node->field.name);
    }

    uirgen->node_to_value.insert(node, field);
    break;
  }
  }
}

void UIRGen::generate() {
  this->ctx->modules.push({});
  this->module = this->ctx->modules.back();
  this->module->init(this->allocator, this->allocator);

  this->node_to_value.init(this->allocator, 32);
  this->builder.module = this->module;
  this->builder.scope = this->module->definitions;
  this->builder.block = nullptr;

  genDeclaration(this, this->ast, this->symbol);
  gen(this, this->ast, this->symbol);
}

void UIRGen::deinit() { this->node_to_value.deinit(); }
