#pragma once

#include "ast.hpp"
#include "containers.hpp"
#include "symbol.hpp"
#include "types.hpp"

struct Value {
  Type *type;
  union {};
};

struct Evaluator {
  Node *ast;
  Scope *scope;
  HashMap<Node *, Type *> type_mapping;

  // VM
  ArrayList<Value *> stack;

  Allocator *allocator;

  void eval();
};
