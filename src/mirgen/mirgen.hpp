#pragma once

#include "../ast.hpp"
#include "../symbol.hpp"
#include "allocator.hpp"
#include "mir.hpp"

struct MIRGen {
  Node *ast;
  Symbol *symbol;
  Allocator *allocator;

  HashMap<Node *, MIRValueId> node_to_value;
  HashMap<Symbol *, MIRScopeId> symbol_to_scope;

  MIRBlock block;
  MIRBuilder builder;
  MIRModule module;
  MIRContext *ctx;

  void generate();
  void deinit();
};
