#include "../print.hpp"
#include "comptime.hpp"
#include <iostream>

Value *execUnary(InteropState *state, Node *node, Symbol *scope) {
  return &node->value;
}

void castAs(Value *out, Value *src, Value *dst) {
  if (src->type->kind == TypeKind::Slice &&
      dst->type->kind == TypeKind::Slice) {
    out->data.text = src->data.text;
    return;
  }

  // TODO: Cast SIMD

  if (src->type->kind == TypeKind::Bool) {
    out->data.integer = src->data.integer;
    return;
  } else if (src->type->kind == TypeKind::Integer) {
    if (dst->type->kind == TypeKind::Float) {
      out->data._float = (double)out->data.integer;
      return;
    }

    out->data.integer = src->data.integer;
    return;
  } else if (src->type->kind == TypeKind::Float) {
    if (dst->type->kind == TypeKind::Integer) {
      out->data.integer = (int64_t)src->data._float;
      return;
    }

    out->data._float = src->data._float;
    return;
  } else if (src->type->kind == TypeKind::Enum) {
    out->data.integer = src->data.integer;
    return;
  }

  std::cerr << "Unhandled `as` cast in comptime\n";
  std::cerr << "Src `" << *src->type << "`\nDst `" << *dst->type << "`\n";
  std::abort();
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
    } else if (out->type->kind == TypeKind::Enum) {
      out->type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out->data._bool = lhs->data.integer == rhs->data.integer;
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
    } else if (out->type->kind == TypeKind::Enum) {
      out->type = state->evaluator->type_cache->get({.kind = TypeKind::Bool});
      out->data._bool = lhs->data.integer != rhs->data.integer;
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
  case Operator::As: {
    out->type = rhs->data.type_value;
    castAs(out, lhs, rhs);
    break;
  }
  case Operator::Bitcast: {
    out->type = rhs->data.type_value;
    out->data = lhs->data;
    break;
  }
  }

  return out;
}
