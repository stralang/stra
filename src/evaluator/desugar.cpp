#include "define.hpp"
#include "evaluator.hpp"

Symbol *desugarForIn(Evaluator *evaluator, Node *node, Symbol *for_scope,
                     Symbol *parent_scope) {
  Node *desugar_node = (Node *)evaluator->allocator->alloc(sizeof(Node));
  Symbol *desugar_scope = (Symbol *)evaluator->allocator->alloc(sizeof(Symbol));

  // Clone
  *desugar_node = *node;
  *desugar_scope = *for_scope;

  // Swap
  Node *tmp_node = node;
  Symbol *tmp_scope = for_scope;
  node = desugar_node;
  for_scope = desugar_scope;
  desugar_node = tmp_node;
  desugar_scope = tmp_scope;

  // Setup node and scope
  memset(desugar_node, 0, sizeof(Node));
  memset(desugar_scope, 0, sizeof(Symbol));

  desugar_node->kind = NodeKind::Compound;
  desugar_node->children.init(evaluator->allocator, 4);
  desugar_node->location = node->_for.conditional->location;
  desugar_scope->init(evaluator->allocator, true, parent_scope);
  desugar_scope->node = desugar_node;

  // Setup index variable
  Node *in_var = (Node *)evaluator->allocator->alloc(sizeof(Node));
  desugar_node->children.push(in_var);
  in_var->kind = NodeKind::Field;
  in_var->location = node->_for.conditional->location;
  in_var->field.name = node->_for.conditional->in.name;
  in_var->field.initial = node->_for.conditional->in.range->range.min;
  in_var->value.type = node->_for.conditional->in.range->range.min->value.type;

  Symbol *in_var_symbol = (Symbol *)evaluator->allocator->alloc(sizeof(Node));
  in_var_symbol->init(evaluator->allocator, false, desugar_scope);
  in_var_symbol->node = in_var;

  // Setup new condition
  auto mode = node->_for.conditional->in.range->range.mode;
  String var_name = node->_for.conditional->in.name;
  Node *max_range = node->_for.conditional->in.range->range.max;
  node->_for.conditional->kind = NodeKind::Operator;
  node->_for.conditional->_operator.rhs = max_range;

  Node *var_name_node = (Node *)evaluator->allocator->alloc(sizeof(Node));
  var_name_node->kind = NodeKind::Name;
  var_name_node->location = node->_for.conditional->location;
  var_name_node->text = var_name;
  node->_for.conditional->_operator.lhs = var_name_node;

  switch (mode) {
  case NodeRange::EqualTo: {
    node->_for.conditional->_operator.opcode = Operator::LessThenOrEqualTo;
    break;
  }
  case NodeRange::LessThan: {
    node->_for.conditional->_operator.opcode = Operator::LessThen;
    break;
  }
  }

  // Setup defer increment
  Node *one_node = (Node *)evaluator->allocator->alloc(sizeof(Node));
  one_node->kind = NodeKind::Integer;
  one_node->integer = 1;

  Node *inc_node = (Node *)evaluator->allocator->alloc(sizeof(Node));
  inc_node->kind = NodeKind::Operator;
  inc_node->_operator.opcode = Operator::Add;
  inc_node->_operator.lhs = var_name_node;
  inc_node->_operator.rhs = one_node;

  Node *assign_node = (Node *)evaluator->allocator->alloc(sizeof(Node));
  assign_node->kind = NodeKind::Operator;
  assign_node->_operator.opcode = Operator::Assign;
  assign_node->_operator.lhs = var_name_node;
  assign_node->_operator.rhs = inc_node;

  Node *defer_inc = (Node *)evaluator->allocator->alloc(sizeof(Node));
  defer_inc->kind = NodeKind::Defer;
  defer_inc->child = assign_node;

  node->_for.body->children.insert(defer_inc, 0);

  // Fix for loop scope
  for_scope->children.remove(0);
  desugar_node->children.push(node);
  desugar_scope->children.push(for_scope);
  for_scope->parent = desugar_scope;
  for_scope->node = node;

  // `in_var` needs to be evaluated here
  evaluate(evaluator, in_var, desugar_scope);

  return for_scope;
}
