#include "evaluator.hpp"
#include "print.hpp"
#include "symbol.hpp"
#include <iostream>

Value *evaluate(Evaluator *evaluator, Node *node, Scope *scope) {
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
      return nullptr;
    }
    break;
  }
  }

  return nullptr;
}

void Evaluator::eval() {
  this->type_mapping.init(this->allocator, 64);
  this->stack.init(this->allocator, 16);

  evaluate(this, this->ast, this->scope);
}
