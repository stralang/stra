#pragma once

#include "../ast.hpp"
#include "../symbol.hpp"
#include "allocator.hpp"
#include "builder.hpp"
#include "mir.hpp"

struct DeferBranch {
  MIRBlock *_return; // Exit function (e.g. `return`, or end of function scope)
  MIRBlock *_scope;  // Exit scope (e.g. `break`, `continue`, or end of scope)
};

struct MIRGen {
  // AST
  Node *ast;
  Symbol *symbol;
  Allocator *allocator;

  // Caches
  HashMap<Node *, MIRValue *> node_to_value;
  HashMap<Symbol *, MIRScope *> symbol_to_scope;

  // Defer
  Node *defer_stack[64];
  size_t defer_stack_len = 0;
  size_t defer_local_boundary = 0; // ifs, loops, etc

  // MIR
  MIRBlock block;
  MIRBuilder builder;
  MIRModule module;
  MIRContext *ctx;

  void generate();
  void deinit();
};
