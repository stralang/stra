#pragma once

#include "ast.hpp"
#include "containers.hpp"
#include "symbol.hpp"
#include "types.hpp"

struct Evaluator {
  Node *ast;
  Scope *scope;

  TypeCache *type_cache;
  HashMap<Node *, Type *> type_mapping;

  // VM
  ArrayList<Value *> stack;

  Allocator *allocator;

  void eval();
};

Value execute(Evaluator *evaluator, Node *node, Scope *scope);
