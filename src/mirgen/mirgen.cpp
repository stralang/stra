#include "mirgen.hpp"
#include "../print.hpp"
#include "define.hpp"
#include "literal.hpp"
#include "mir.hpp"
#include <iostream>

MIRValue *addr(MIRGen *mirgen, Node *node, Symbol *scope) {
  switch (node->kind) {
  case NodeKind::Name: {
    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    MIRValue **value = mirgen->node_to_value.get(symbol->node);
    if (value == nullptr) {
      gen(mirgen, symbol->node, symbol->parent);
      value = mirgen->node_to_value.get(symbol->node);
    }

    if (value == nullptr) {
      std::cerr << node->location << " Couldn't find value `" << node->text
                << "`. Aborting...\n";
      std::abort();
    }

    return *value;
  }
  case NodeKind::Operator: {
    if (node->_operator.opcode == Operator::MemberAccess) {
      return addrMemberAccess(mirgen, node, scope);
    }
    break;
  }
  }

  return nullptr;
}

MIRValue *gen(MIRGen *mirgen, Node *node, Symbol *scope) {
  switch (node->kind) {
  case NodeKind::Compound: {
    for (size_t i = 0; i < node->children.length; i++) {
      gen(mirgen, node->children.getUnchecked(i), scope);
    }
    break;
  }
  case NodeKind::Block: {
    Symbol *block_scope = scope->findSymbolByNode(node);

    for (size_t i = 0; i < node->children.length; i++) {
      gen(mirgen, node->children.getUnchecked(i), block_scope);
    }
    break;
  }
  case NodeKind::Name: {
    MIRValue *builtin = genBuiltin(mirgen, node->text);
    if (builtin != nullptr) {
      return builtin;
    }

    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    MIRValue **value = mirgen->node_to_value.get(symbol->node);
    if (value == nullptr) {
      gen(mirgen, symbol->node, symbol->parent);
      value = mirgen->node_to_value.get(symbol->node);
    }

    if (value == nullptr) {
      std::cerr << node->location << " Couldn't find value `" << node->text
                << "`. Aborting...\n";
      std::abort();
    }

    return mirgen->builder.buildLoad(*value, {.ptr = nullptr});
  }
  case NodeKind::Value: {
    return valueToMIR(mirgen, &node->value);
  }
  case NodeKind::Field: {
    MIRValue **cache = mirgen->node_to_value.get(node);
    if (cache != nullptr) {
      return *cache;
    }

    // Get Symbol
    Symbol *field_symbol = scope->findSymbolByNode(node);

    // TODO: Get Name

    // Generate field
    MIRValue *type = nullptr;
    MIRValue *value = nullptr;
    if (node->field.type != nullptr) {
      type = gen(mirgen, node->field.type, field_symbol);
    }
    if (node->field.initial != nullptr) {
      value = gen(mirgen, node->field.initial, field_symbol);
    } else if (!node->field.undefined) {
      // TODO: Get default value for type
    }

    MIRValue *field = mirgen->builder.buildField(type, value, node->field.name);
    mirgen->node_to_value.insert(node, field);
    return field;
  }
  case NodeKind::Function: {
    Symbol *fn_symbol = scope->findSymbolByNode(node);

    MIRValue *value = mirgen->ctx->make({.kind = MIRValueKind::Function});

    // Parameter Types
    value->function.parameter_types.len = node->function.parameters.length;
    value->function.parameter_types.ptr = (MIRValue **)mirgen->allocator->alloc(
        sizeof(MIRValue *) * node->function.parameters.length);

    for (size_t i = 0; i < node->function.parameters.length; i++) {
      Node *arg = node->function.parameters.getUnchecked(i);
      value->function.parameter_types.ptr[i] =
          gen(mirgen, arg->field.type, fn_symbol);
    }

    // Return Type
    if (node->function.return_type == nullptr) {
      MIRValue void_ty = {.kind = MIRValueKind::Literal};
      void_ty.literal.lit_type =
          mirgen->ctx->type_cache->get({.kind = TypeKind::TypeId});
      void_ty.literal._typeid =
          mirgen->ctx->type_cache->get({.kind = TypeKind::Void});
      void_ty.result_type = void_ty.literal.lit_type;

      value->function.return_type = mirgen->ctx->make(void_ty);
    } else {
      value->function.return_type =
          gen(mirgen, node->function.return_type, fn_symbol);
    }

    // Body
    if (node->function.body != nullptr) {
      value->function.blocks.init(mirgen->allocator, 32);
      value->function.blocks.push({});

      MIRBlock *entry = value->function.blocks.back();
      entry->function = &value->function;
      entry->instructions.init(mirgen->allocator, 32);

      MIRBlock *prev_block = mirgen->builder.block;
      mirgen->builder.block = entry;

      // Parameters
      for (size_t i = 0; i < node->function.parameters.length; i++) {
        Node *arg = node->function.parameters.getUnchecked(i);
        MIRValue *mir_arg = mirgen->builder.buildArg(
            value->function.parameter_types.ptr[i], arg->field.name);
        mirgen->node_to_value.insert(arg, mir_arg);
      }

      // Body
      gen(mirgen, node->function.body, fn_symbol);

      // End
      mirgen->builder.block = prev_block;
    }

    return value;
  }
  case NodeKind::Assignment: {
    return genAssignment(mirgen, node, scope);
  }
  case NodeKind::UnaryOperator: {
    return genUnary(mirgen, node, scope);
  }
  case NodeKind::Operator: {
    return genBinary(mirgen, node, scope);
  }
  case NodeKind::Call: {
    MIRValue *callee = gen(mirgen, node->call.callee, scope);
    Slice<MIRValue *> arguments = {
        .ptr = (MIRValue **)mirgen->allocator->alloc(
            sizeof(MIRValue *) * node->call.arguments.length),
        .len = node->call.arguments.length,
    };

    // Receiver
    MIRValue *receiver = nullptr;
    if (node->call.callee->_operator.opcode == Operator::MemberAccess) {
      receiver = gen(mirgen, node->call.callee->_operator.lhs, scope);
    }

    // Arguments
    for (size_t i = 0; i < arguments.len; i++) {
      arguments.ptr[i] =
          gen(mirgen, node->call.arguments.getUnchecked(i), scope);
    }

    return mirgen->builder.buildCall(callee, arguments, receiver,
                                     {.ptr = nullptr});
  }
  case NodeKind::Return: {
    MIRValue *ret_value = nullptr;
    if (node->child != nullptr) {
      ret_value = gen(mirgen, node->child, scope);
    }

    return mirgen->builder.buildReturn(ret_value);
  }
  }

  return nullptr;
}

void MIRGen::generate() {
  this->node_to_value.init(this->allocator, 32);
  this->module.init(this->allocator);
  this->builder.module = &this->module;

  gen(this, this->ast, this->symbol);

  printMIRModule(&this->module);
}

void MIRGen::deinit() {
  this->node_to_value.deinit();
  this->module.instructions.deinit();
}
