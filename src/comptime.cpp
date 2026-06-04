#include "ast.hpp"
#include "containers.hpp"
#include "evaluator.hpp"
#include "helper.hpp"
#include "operator.hpp"
#include "print.hpp"
#include "symbol.hpp"
#include <cstring>
#include <iostream>

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

extern void evaluate(Evaluator *evaluator, Node *node, Symbol *scope);
Value *exec(InteropState *state, Node *node, Symbol *scope);

Value *execUnary(InteropState *state, Node *node, Symbol *scope) {
  return &node->value;
}

Value *execBinary(InteropState *state, Node *node, Symbol *scope) {
  Value *lhs = exec(state, node->_operator.lhs, scope);
  Value *rhs = exec(state, node->_operator.rhs, scope);

  Value *out = &node->value;
  out->type = lhs->type;
  out->has_data = true;

  switch (node->_operator.opcode) {
  case Operator::Assign: {
    lhs->data = rhs->data;
    break;
  }
  case Operator::Add: {
    if (out->type->kind == TypeKind::Integer) {
      out->data.integer = lhs->data.integer + rhs->data.integer;
    } else if (out->type->kind == TypeKind::Float) {
      out->data._float = lhs->data._float + rhs->data._float;
    } else {
      std::cerr << "Addition for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Sub: {
    if (out->type->kind == TypeKind::Integer) {
      out->data.integer = lhs->data.integer - rhs->data.integer;
    } else if (out->type->kind == TypeKind::Float) {
      out->data._float = lhs->data._float - rhs->data._float;
    } else {
      std::cerr << "Subtraction for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Mul: {
    if (out->type->kind == TypeKind::Integer) {
      out->data.integer = lhs->data.integer * rhs->data.integer;
    } else if (out->type->kind == TypeKind::Float) {
      out->data._float = lhs->data._float * rhs->data._float;
    } else {
      std::cerr << "Multiplication for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Div: {
    if (out->type->kind == TypeKind::Integer) {
      out->data.integer = lhs->data.integer / rhs->data.integer;
    } else if (out->type->kind == TypeKind::Float) {
      out->data._float = lhs->data._float / rhs->data._float;
    } else {
      std::cerr << "Division for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Mod: {
    if (out->type->kind == TypeKind::Integer) {
      out->data.integer = lhs->data.integer % rhs->data.integer;
    } else {
      std::cerr << "Modulo for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Bitwise_Or: {
    if (out->type->kind == TypeKind::Integer) {
      out->data.integer = lhs->data.integer | rhs->data.integer;
    } else {
      std::cerr << "Modulo for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Bitwise_Xor: {
    if (out->type->kind == TypeKind::Integer) {
      out->data.integer = lhs->data.integer ^ rhs->data.integer;
    } else {
      std::cerr << "Modulo for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Bitwise_And: {
    if (out->type->kind == TypeKind::Integer) {
      out->data.integer = lhs->data.integer & rhs->data.integer;
    } else {
      std::cerr << "Modulo for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Bitwise_LeftShift: {
    if (out->type->kind == TypeKind::Integer) {
      out->data.integer = lhs->data.integer << rhs->data.integer;
    } else {
      std::cerr << "Modulo for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Bitwise_RightShift: {
    if (out->type->kind == TypeKind::Integer) {
      out->data.integer = lhs->data.integer >> rhs->data.integer;
    } else {
      std::cerr << "Modulo for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Logical_Or: {
    if (out->type->kind == TypeKind::Bool) {
      out->data._bool = lhs->data._bool || rhs->data._bool;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Logical_And: {
    if (out->type->kind == TypeKind::Bool) {
      out->data._bool = lhs->data._bool && rhs->data._bool;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::EqualTo: {
    if (out->type->kind == TypeKind::Bool) {
      out->data._bool = lhs->data._bool == rhs->data._bool;
    } else if (out->type->kind == TypeKind::Integer) {
      out->type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out->data._bool = lhs->data.integer == rhs->data.integer;
    } else if (out->type->kind == TypeKind::Float) {
      out->type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out->data._bool = lhs->data._float == rhs->data._float;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::NotEqualTo: {
    if (out->type->kind == TypeKind::Bool) {
      out->data._bool = lhs->data._bool != rhs->data._bool;
    } else if (out->type->kind == TypeKind::Integer) {
      out->type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out->data._bool = lhs->data.integer != rhs->data.integer;
    } else if (out->type->kind == TypeKind::Float) {
      out->type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out->data._bool = lhs->data._float != rhs->data._float;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::LessThen: {
    if (out->type->kind == TypeKind::Integer) {
      out->type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out->data._bool = lhs->data.integer < rhs->data.integer;
    } else if (out->type->kind == TypeKind::Float) {
      out->type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out->data._bool = lhs->data._float < rhs->data._float;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::GreaterThen: {
    if (out->type->kind == TypeKind::Integer) {
      out->type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out->data._bool = lhs->data.integer > rhs->data.integer;
    } else if (out->type->kind == TypeKind::Float) {
      out->type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out->data._bool = lhs->data._float > rhs->data._float;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::LessThenOrEqualTo: {
    if (out->type->kind == TypeKind::Integer) {
      out->type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out->data._bool = lhs->data.integer <= rhs->data.integer;
    } else if (out->type->kind == TypeKind::Float) {
      out->type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out->data._bool = lhs->data._float <= rhs->data._float;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::GreaterThenOrEqualTo: {
    if (out->type->kind == TypeKind::Integer) {
      out->type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out->data._bool = lhs->data.integer <= rhs->data.integer;
    } else if (out->type->kind == TypeKind::Float) {
      out->type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out->data._bool = lhs->data._float <= rhs->data._float;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out->type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  }

  return out;
}

Value *exec(InteropState *state, Node *node, Symbol *scope) {
  state->steps += 1;
  state->depth += 1;
  if (state->steps > state->max_steps) {
    std::cerr << "Compile-time execution exceeded `" << state->max_steps
              << "` steps. Aborting\n";
    std::abort();
  }

  Value *out = nullptr;

  switch (node->kind) {
  case NodeKind::Compound: {
    Symbol *compound_scope = scope->findSymbolByNode(node);
    if (compound_scope == nullptr) {
      compound_scope = scope;
    }

    for (size_t i = 0; i < node->children.length; i++) {
      Value *result = exec(state, node->children.data.ptr[i], compound_scope);

      if (state->_return) {
        out = result;
        break;
      }
    }
    break;
  }
  case NodeKind::Name: {
    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    if (symbol != nullptr) {
      out = &symbol->node->value;
    } else if (node->value.type != nullptr && node->value.has_data) {
      out = &node->value;
    } else {
      std::cerr << node->location << " Symbol not Found: `" << node->text
                << "`. Aborting\n";
      std::abort();
    }
    break;
  }
  case NodeKind::Bool:
  case NodeKind::Integer:
  case NodeKind::Float:
  case NodeKind::Char:
  case NodeKind::String: {
    if (!node->value.has_data) {
      std::cerr << "Literal `" << node->kind
                << "` does not have data. Aborting\n";
      std::abort();
    }
    out = &node->value;
    break;
  }
  case NodeKind::Field: {
    Symbol *symbol = scope->findSymbolByNode(node);
    Value value = node->value;

    if (!value.has_data) {
      if (node->field.initial != nullptr) {
        value = *exec(state, node->field.initial, symbol);
      } else {
        memset(&value.data, 0, sizeof(value.data));
      }
    }

    state->ret_stack.push({.ast = symbol->node});
    out = &node->value;
    break;
  }
    // TODO: ...
  case NodeKind::Function: {
    out = &node->value;
    break;
  }
  case NodeKind::Struct:
  case NodeKind::Enum:
  case NodeKind::Union: {
    Node *copy = astCopy(state->evaluator->allocator, node, scope);
    evaluate(state->evaluator, copy, scope); // Re-evaluate
    out = &copy->value;
    break;
  }
  // TODO: ...
  case NodeKind::UnaryOperator: {
    out = execUnary(state, node, scope);
    break;
  }
  case NodeKind::Operator: {
    out = execBinary(state, node, scope);
    break;
  }
  case NodeKind::Call: {
    // Save State
    size_t state_offset = *state->fn_stack_boundary.back();
    for (size_t i = state_offset; i < state->ret_stack.length; i++) {
      InteropState::RetStackNode stack = state->ret_stack.data[i];
      stack.backup = stack.ast->value;
    }

    state->fn_stack_boundary.push(state->ret_stack.length);

    // Get function
    Value *fn_value = exec(state, node->call.callee, scope);
    Symbol *fn_symbol = fn_value->type->function.scope;
    Node *fn_node = fn_symbol->node;

    // Apply arguments
    for (size_t i = 0; i < node->call.arguments.length; i++) {
      Node *arg = node->call.arguments.data.ptr[i];
      Symbol *arg_symbol = scope->findSymbolByNode(arg);
      Value *arg_value = &arg->value;
      if (!arg_value->has_data) {
        std::cerr << arg->location
                  << " Cannot execute call with runtime value. Aborting\n";
        std::abort();
      }

      Node *param_node = fn_node->function.parameters.data[i];
      Symbol *param_symbol = fn_symbol->findSymbolByNode(param_node);
      param_node->value = *arg_value;
      state->ret_stack.push({.ast = param_node});
    }

    // Execute call
    out = exec(state, fn_node->function.body, fn_symbol);

    // Handle Return
    if (out == nullptr) {
      node->value.type =
          state->evaluator->type_cache->get({.kind = TypeKind::Void});
      node->value.has_data = false;
    } else {
      node->value = *out;
    }

    out = &node->value;
    state->_return = false;

    // Load State
    state->fn_stack_boundary.pop();
    for (size_t i = state_offset; i < state->ret_stack.length; i++) {
      InteropState::RetStackNode stack = state->ret_stack.data[i];
      if (stack.backup.type == nullptr) {
        // State wasn't saved
        break;
      }

      stack.ast->value = stack.backup;
    }
    break;
  }
  // TODO: ...
  case NodeKind::Return: {
    if (node->child == nullptr) {
      node->value.type =
          state->evaluator->type_cache->get({.kind = TypeKind::Void});
      node->value.has_data = false;
    } else {
      node->value = *exec(state, node->child, scope);
    }

    out = &node->value;
    state->_return = true;
    break;
  }
  case NodeKind::Comptime: {
    out = exec(state, node->child, scope);
    break;
  }
  }

  state->depth -= 1;
  return out;
}

Value execute(Evaluator *evaluator, Node *node, Symbol *scope) {
  InteropState state = {
      .evaluator = evaluator,
  };

  // Setup Return stack
  state.ret_stack.init(evaluator->allocator, 16);
  state.fn_stack_boundary.init(evaluator->allocator, 256);
  state.fn_stack_boundary.push(0);

  // Execute
  Value out = *exec(&state, node, scope);

  // Apply
  node->value = out;
  switch (out.type->kind) {
  case TypeKind::Bool: {
    node->kind = NodeKind::Bool;
    node->integer = out.data._bool;
    break;
  }
  case TypeKind::Integer: {
    node->kind = NodeKind::Integer;
    node->integer = out.data.integer;
    break;
  }
  case TypeKind::Float: {
    node->kind = NodeKind::Float;
    node->_float = out.data._float;
    break;
  }
  }

  // Cleanup
  state.ret_stack.deinit();

  return out;
}
