#include "evaluator.hpp"
#include "operator.hpp"
#include "print.hpp"
#include <iostream>

struct InteropState {
  Evaluator *evaluator;

  size_t steps = 0;
  size_t depth = 0;
  size_t max_steps = 1000000;
};

Value exec(InteropState *state, Node *node, Symbol *scope);

Value execUnary(InteropState *state, Node *node, Symbol *scope) {
  return Value{.type = nullptr, .has_data = false};
}

Value execBinary(InteropState *state, Node *node, Symbol *scope) {
  exec(state, node->_operator.lhs, scope);
  exec(state, node->_operator.rhs, scope);

  Value out = {.type = node->_operator.lhs->value.type, .has_data = true};

  switch (node->_operator.opcode) {
  case Operator::Assign: {
    // TODO: Assign
    std::cerr << "TODO: Assign\n";
    std::abort();
  }
  case Operator::Add: {
    if (out.type->kind == TypeKind::Integer) {
      out.data.integer = node->_operator.lhs->value.data.integer +
                         node->_operator.rhs->value.data.integer;
    } else if (out.type->kind == TypeKind::Float) {
      out.data._float = node->_operator.lhs->value.data._float +
                        node->_operator.rhs->value.data._float;
    } else {
      std::cerr << "Addition for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Sub: {
    if (out.type->kind == TypeKind::Integer) {
      out.data.integer = node->_operator.lhs->value.data.integer -
                         node->_operator.rhs->value.data.integer;
    } else if (out.type->kind == TypeKind::Float) {
      out.data._float = node->_operator.lhs->value.data._float -
                        node->_operator.rhs->value.data._float;
    } else {
      std::cerr << "Subtraction for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Mul: {
    if (out.type->kind == TypeKind::Integer) {
      out.data.integer = node->_operator.lhs->value.data.integer *
                         node->_operator.rhs->value.data.integer;
    } else if (out.type->kind == TypeKind::Float) {
      out.data._float = node->_operator.lhs->value.data._float *
                        node->_operator.rhs->value.data._float;
    } else {
      std::cerr << "Multiplication for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Div: {
    if (out.type->kind == TypeKind::Integer) {
      out.data.integer = node->_operator.lhs->value.data.integer /
                         node->_operator.rhs->value.data.integer;
    } else if (out.type->kind == TypeKind::Float) {
      out.data._float = node->_operator.lhs->value.data._float /
                        node->_operator.rhs->value.data._float;
    } else {
      std::cerr << "Division for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Mod: {
    if (out.type->kind == TypeKind::Integer) {
      out.data.integer = node->_operator.lhs->value.data.integer %
                         node->_operator.rhs->value.data.integer;
    } else {
      std::cerr << "Modulo for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Bitwise_Or: {
    if (out.type->kind == TypeKind::Integer) {
      out.data.integer = node->_operator.lhs->value.data.integer |
                         node->_operator.rhs->value.data.integer;
    } else {
      std::cerr << "Modulo for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Bitwise_Xor: {
    if (out.type->kind == TypeKind::Integer) {
      out.data.integer = node->_operator.lhs->value.data.integer ^
                         node->_operator.rhs->value.data.integer;
    } else {
      std::cerr << "Modulo for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Bitwise_And: {
    if (out.type->kind == TypeKind::Integer) {
      out.data.integer = node->_operator.lhs->value.data.integer &
                         node->_operator.rhs->value.data.integer;
    } else {
      std::cerr << "Modulo for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Bitwise_LeftShift: {
    if (out.type->kind == TypeKind::Integer) {
      out.data.integer = node->_operator.lhs->value.data.integer
                         << node->_operator.rhs->value.data.integer;
    } else {
      std::cerr << "Modulo for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Bitwise_RightShift: {
    if (out.type->kind == TypeKind::Integer) {
      out.data.integer = node->_operator.lhs->value.data.integer >>
                         node->_operator.rhs->value.data.integer;
    } else {
      std::cerr << "Modulo for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Logical_Or: {
    if (out.type->kind == TypeKind::Bool) {
      out.data._bool = node->_operator.lhs->value.data._bool ||
                       node->_operator.rhs->value.data._bool;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::Logical_And: {
    if (out.type->kind == TypeKind::Bool) {
      out.data._bool = node->_operator.lhs->value.data._bool &&
                       node->_operator.rhs->value.data._bool;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::EqualTo: {
    if (out.type->kind == TypeKind::Bool) {
      out.data._bool = node->_operator.lhs->value.data._bool ==
                       node->_operator.rhs->value.data._bool;
    } else if (out.type->kind == TypeKind::Integer) {
      out.type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out.data._bool = node->_operator.lhs->value.data.integer ==
                       node->_operator.rhs->value.data.integer;
    } else if (out.type->kind == TypeKind::Float) {
      out.type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out.data._bool = node->_operator.lhs->value.data._float ==
                       node->_operator.rhs->value.data._float;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::NotEqualTo: {
    if (out.type->kind == TypeKind::Bool) {
      out.data._bool = node->_operator.lhs->value.data._bool !=
                       node->_operator.rhs->value.data._bool;
    } else if (out.type->kind == TypeKind::Integer) {
      out.type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out.data._bool = node->_operator.lhs->value.data.integer !=
                       node->_operator.rhs->value.data.integer;
    } else if (out.type->kind == TypeKind::Float) {
      out.type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out.data._bool = node->_operator.lhs->value.data._float !=
                       node->_operator.rhs->value.data._float;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::LessThen: {
    if (out.type->kind == TypeKind::Integer) {
      out.type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out.data._bool = node->_operator.lhs->value.data.integer <
                       node->_operator.rhs->value.data.integer;
    } else if (out.type->kind == TypeKind::Float) {
      out.type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out.data._bool = node->_operator.lhs->value.data._float <
                       node->_operator.rhs->value.data._float;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::GreaterThen: {
    if (out.type->kind == TypeKind::Integer) {
      out.type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out.data._bool = node->_operator.lhs->value.data.integer >
                       node->_operator.rhs->value.data.integer;
    } else if (out.type->kind == TypeKind::Float) {
      out.type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out.data._bool = node->_operator.lhs->value.data._float >
                       node->_operator.rhs->value.data._float;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::LessThenOrEqualTo: {
    if (out.type->kind == TypeKind::Integer) {
      out.type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out.data._bool = node->_operator.lhs->value.data.integer <=
                       node->_operator.rhs->value.data.integer;
    } else if (out.type->kind == TypeKind::Float) {
      out.type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out.data._bool = node->_operator.lhs->value.data._float <=
                       node->_operator.rhs->value.data._float;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  case Operator::GreaterThenOrEqualTo: {
    if (out.type->kind == TypeKind::Integer) {
      out.type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out.data._bool = node->_operator.lhs->value.data.integer <=
                       node->_operator.rhs->value.data.integer;
    } else if (out.type->kind == TypeKind::Float) {
      out.type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out.data._bool = node->_operator.lhs->value.data._float <=
                       node->_operator.rhs->value.data._float;
    } else {
      std::cerr << node->_operator.opcode << " for `" << *out.type
                << "` is not implemented. Aborting\n";
      std::abort();
    }
    break;
  }
  }

  return out;
}

Value exec(InteropState *state, Node *node, Symbol *scope) {
  state->steps += 1;
  state->depth += 1;
  if (state->steps > state->max_steps) {
    std::cerr << "Compile-time execution exceeded `" << state->max_steps
              << "` steps. Aborting\n";
    std::abort();
  }

  Value out = {.type = nullptr, .has_data = false};

  switch (node->kind) {
  case NodeKind::Compound: {
    Symbol *compound_scope = scope->findSymbolByNode(node);
    if (compound_scope == nullptr) {
      compound_scope = scope;
    }

    for (size_t i = 0; i < node->children.length; i++) {
      exec(state, node->children.data.ptr[i], compound_scope);
    }
    break;
  }
  case NodeKind::Name: {
    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    if (symbol == nullptr) {
      std::cerr << node->location << " Symbol not Found: `" << node->text
                << "`. Aborting\n";
      std::abort();
    }

    if (symbol->node->value.type == nullptr || !symbol->node->value.has_data) {
      std::cerr << node->location << " Symbol without value. Aborting\n";
      std::abort();
    }

    out = symbol->node->value;
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
    out = node->value;
    break;
  }
  // TODO: ...
  case NodeKind::UnaryOperator: {
    out = execUnary(state, node, scope);
    break;
  }
  case NodeKind::Operator: {
    out = execBinary(state, node, scope);
    if (out.has_data) {
      if (out.type->kind == TypeKind::Bool) {
        node->kind = NodeKind::Bool;
      } else if (out.type->kind == TypeKind::Integer) {
        node->kind = NodeKind::Integer;
        node->integer = node->value.data.integer;
      } else if (out.type->kind == TypeKind::Float) {
        node->kind = NodeKind::Float;
        node->_float = node->value.data._float;
      }
    }
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

  return exec(&state, node, scope);
}
