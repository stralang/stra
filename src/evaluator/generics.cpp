#include "../helper.hpp"
#include "../print.hpp"
#include "define.hpp"
#include <iostream>

void specializeCall(Evaluator *evaluator, Node *call_node, Symbol *call_scope,
                    Node *fn_node, Symbol *fn_scope) {
  // Get arguments and populate compile-time arguments
  Hasher hasher;
  hasher.hash(fn_scope);
  for (size_t i = 0; i < call_node->call.arguments.length; i++) {
    hasher.hash(&i);

    Node *arg = call_node->call.arguments.data.ptr[i];
    Node *param = fn_node->function.parameters.data[i];
    if (param->field.comptime) {
      hasher.hash(&arg->value.data);
    }
  }

  // TODO: Cached function

  // Copy function
  Symbol *parent_scope = fn_scope->parent;
  if (parent_scope->node->kind == NodeKind::Field) {
    parent_scope = parent_scope->parent;
  }

  Node *new_fn_node = astCopy(evaluator->allocator, fn_node, parent_scope);
  Symbol *new_fn_scope = parent_scope->findSymbolByNode(new_fn_node);
  parent_scope->node->children.push(new_fn_node);
  new_fn_node->function.polymorphic = false;

  for (size_t i = 0; i < call_node->call.arguments.length; i++) {
    Node *arg = call_node->call.arguments.data.ptr[i];
    Node *param = new_fn_node->function.parameters.data[i];
    if (param->field.comptime) {
      param->value.has_data = true;
      param->value.data = arg->value.data;
    }
  }

  // Evaluate function
  evaluate(evaluator, new_fn_node, new_fn_scope);

  // Remove Compile-time arguments
  Type new_fn_type = *new_fn_node->value.type;
  for (size_t l = call_node->call.arguments.length; l > 0; l--) {
    size_t i = l - 1;
    Node *param = new_fn_node->function.parameters.data[i];
    if (param->field.comptime) {
      new_fn_node->function.parameters.remove(i);
      call_node->call.arguments.remove(i);
      new_fn_type.function.arguments.remove(i);
    }
  }

  new_fn_node->value.type = evaluator->type_cache->get(new_fn_type);
  new_fn_node->value.has_data = true;
  new_fn_node->value.data.symbol = new_fn_scope;
  call_node->call.callee->value = new_fn_node->value;

  // Cache function
  // TODO: Cache function
}
