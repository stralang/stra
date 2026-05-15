#pragma once

#include "ast.hpp"
#include "containers.hpp"
#include "symbol.hpp"
#include "types.hpp"

struct Value {
  Type *type;
  bool has_value;
  union {
    Type *type_value;
    String text;
    int64_t integer;
    double _float;
    Node *node;
  } data;
};

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
