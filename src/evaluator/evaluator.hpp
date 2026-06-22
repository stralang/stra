#pragma once

#include "../ast.hpp"
#include "../containers.hpp"
#include "../environment.hpp"
#include "../symbol.hpp"
#include "../types.hpp"

struct Evaluator {
  Node *ast;
  Symbol *symbol;

  TypeCache *type_cache;
  HashMap<Node *, Type *> type_mapping;

  Environment *environment;

  // VM
  ArrayList<Value *> stack;

  size_t error_count = 0;
  size_t warning_count = 0;
  void (*error_func)(SrcLoc srcloc, String msg);
  void (*warning_func)(SrcLoc srcloc, String msg);

  Allocator *allocator;

  void eval();
};
