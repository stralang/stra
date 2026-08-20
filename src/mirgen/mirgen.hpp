#pragma once

#include "../ast.hpp"
#include "../symbol.hpp"
#include "allocator.hpp"
#include "mir.hpp"

struct MIRGen {
  Node *ast;
  Symbol *symbol;
  Allocator *allocator;

  HashMap<Node *, MIRValue *> node_to_value;
  HashMap<Symbol *, MIRScope *> symbol_to_scope;

  MIRBlock block;
  MIRBuilder builder;
  MIRModule module;
  MIRContext *ctx;

  void generate();
  void deinit();
};
