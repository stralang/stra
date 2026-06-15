#include "../print.hpp"
#include "define.hpp"

void evaluateFunction(Evaluator *evaluator, Node *node, Symbol *scope) {
  Symbol *fn_scope = scope->findSymbolByNode(node);
  node->value.has_data = false;

  // Prepare type
  Type *fn_t = evaluator->type_cache->get(
      {.kind = TypeKind::Function, .function = {.scope = fn_scope}});
  node->value.type = fn_t;
  fn_t->function.arguments.init(evaluator->allocator, 4);

  // Evaluate parameters
  for (size_t i = 0; i < node->function.parameters.length; i++) {
    Node *param = node->function.parameters.data.ptr[i];
    evaluate(evaluator, param, fn_scope);
    Value *val = &param->value;
    expect(val->type != nullptr, param->location,
           "Failed to evaluate function parameter");
    fn_t->function.arguments.push(val->type);
  }

  // Evaluate return type
  if (node->function.return_type != nullptr) {
    evaluate(evaluator, node->function.return_type, scope);
    Value *val = &node->function.return_type->value;
    expect(val->type != nullptr, node->function.return_type->location,
           "Failed to evaluate function return type");
    expect(val->type->kind == TypeKind::TypeId,
           node->function.return_type->location,
           "Function return type must be a type");
    fn_t->function.return_type = val->data.type_value;
  } else {
    fn_t->function.return_type =
        evaluator->type_cache->get({.kind = TypeKind::Void});
  }

  // Evaluate Body
  if (node->function.body != nullptr) {
    evaluate(evaluator, node->function.body, fn_scope);
  } else if (!node->function.undefined) {
    node->value.has_data = true;
    node->value.data.type_value = node->value.type;
    node->value.type = evaluator->type_cache->get({.kind = TypeKind::TypeId});
  }
}

void evaluateCall(Evaluator *evaluator, Node *node, Symbol *scope) {
  Node *callee = node->call.callee;
  evaluate(evaluator, callee, scope);

  // Auto dereference
  Type *callee_type = callee->value.type;
  if (callee->value.type->kind == TypeKind::Pointer) {
    callee_type = callee->value.type->child;
  }

  // Get function
  expect(callee_type->kind == TypeKind::Function, callee->location,
         "Callee must be a function. Got `" << *callee_type << "`");

  Type *fn_type = callee_type;
  Symbol *fn_scope = fn_type->function.scope;
  Node *method = scope->node;

  // Get receiver
  Node *receiver = nullptr;
  size_t initial_idx = 0;
  if (callee->kind == NodeKind::Operator &&
      callee->_operator.opcode == Operator::MemberAccess &&
      callee->_operator.lhs->value.type->kind != TypeKind::TypeId) {
    receiver = callee->_operator.lhs;
    expect(fn_type->function.arguments.length >= 1, receiver->location,
           "Receiver expects method with atleast 1 argument");

    Type *expected_type = fn_type->function.arguments.data.ptr[0];
    Type *receiver_type = receiver->value.type;
    if (expected_type->kind == TypeKind::Pointer &&
        receiver_type->kind != TypeKind::Pointer) {
      expect(compareTypes(expected_type->child, receiver_type),
             receiver->location,
             " Receiver `" << *receiver->value.type
                           << "` cannot auto reference to `" << *expected_type
                           << "`");
    } else {
      expect(compareTypes(expected_type, receiver_type), receiver->location,
             "Receiver `" << *receiver->value.type << "` doesn't match `"
                          << *expected_type << "`");
    }
    initial_idx = 1;
  }

  // Evaluate arguments
  for (size_t i = 0; i < node->call.arguments.length; i++) {
    Node *arg = node->call.arguments.data.ptr[i];
    if (i > fn_type->function.arguments.length - initial_idx) {
      std::cerr << arg->location << " Too many arguments\n";
      node->value.type = nullptr;
      return;
    }

    Type *expected_type = fn_type->function.arguments.data.ptr[i + initial_idx];

    evaluate(evaluator, arg, scope);
    arg->value.type = autoConvert(evaluator, arg->value.type, expected_type);

    expect(compareTypes(expected_type, arg->value.type), arg->location,
           "Argument `" << *arg->value.type << "` doesn't match expected `"
                        << *expected_type << "`");
  }

  node->value.type = fn_type->function.return_type;
  node->value.has_data = false;
}
