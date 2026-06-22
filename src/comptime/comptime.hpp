#pragma once

#include "../ast.hpp"
#include "../evaluator/evaluator.hpp"
#include "../symbol.hpp"

struct InteropState {
  Evaluator *evaluator;

  size_t steps = 0;
  size_t depth = 0;
  size_t max_steps = 1000000;

  struct RetStackNode {
    Node *ast;
    Value backup;
  };

  ArrayList<RetStackNode> ret_stack;
  ArrayList<size_t> fn_stack_boundary;

  bool _return = false;
};

Value execute(Evaluator *evaluator, Node *node, Symbol *scope);

Value *exec(InteropState *state, Node *node, Symbol *scope);
Value *execUnary(InteropState *state, Node *node, Symbol *scope);
Value *execBinary(InteropState *state, Node *node, Symbol *scope);
