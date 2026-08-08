#include "mirgen.hpp"
#include "../print.hpp"
#include "define.hpp"
#include "mir.hpp"

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

    return mirgen->builder.buildLoad(*value);
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
    }

    MIRValue *field = mirgen->builder.buildField(type, value);
    mirgen->node_to_value.insert(node, field);
    return field;
  }
  }

  return nullptr;
}

void MIRGen::generate() {
  this->node_to_value.init(this->allocator, 32);
  this->module.init(this->allocator);
  this->builder.module = &this->module;

  gen(this, this->ast, this->symbol);

  this->module.print();
}

void MIRGen::deinit() {
  this->node_to_value.deinit();
  this->module.instructions.deinit();
}
