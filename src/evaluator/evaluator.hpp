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
  HashMap<uint64_t, Symbol *> specialized_cache;

  Environment *environment;

  size_t error_count = 0;
  size_t warning_count = 0;
  void (*error_func)(SrcLoc srcloc, String msg);
  void (*warning_func)(SrcLoc srcloc, String msg);

  Allocator *allocator;

  void eval();
};
