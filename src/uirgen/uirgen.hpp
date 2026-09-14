#pragma once

#include "../ast.hpp"
#include "../symbol.hpp"
#include "allocator.hpp"
#include "builder.hpp"
#include "uir.hpp"

struct DeferBranch {
  UIRBlock *_return; // Exit function (e.g. `return`, or end of function scope)
  UIRBlock *_scope;  // Exit scope (e.g. `break`, `continue`, or end of scope)
};

struct UIRGen {
  // AST
  Node *ast;
  Symbol *symbol;
  Allocator *allocator;

  // Caches
  HashMap<Node *, UIRValue *> node_to_value;

  // Defer
  Node *defer_stack[64];
  size_t defer_stack_len = 0;
  size_t defer_local_boundary = 0; // ifs, loops, etc

  // UIR
  UIRBlock block;
  UIRBuilder builder;
  UIRModule *module;
  UIRContext *ctx;

  // Errors
  size_t error_count = 0;
  size_t warning_count = 0;
  void (*error_func)(SrcLoc srcloc, String msg);
  void (*warning_func)(SrcLoc srcloc, String msg);

  void generate();
  void deinit();
};
