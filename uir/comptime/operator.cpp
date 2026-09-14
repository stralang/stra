#include "define.hpp"
#include "literal.hpp"
#include "uir.hpp"
#include <cmath>
#include <iostream>

UIRLiteral executeBinary(UIRComptime *state, UIRModule *module,
                         ComptimeStackFrame *frame, UIRValue *inst) {
  UIRLiteral lhs = state->getValue(frame, module, inst->binop.lhs);
  UIRLiteral rhs = state->getValue(frame, module, inst->binop.rhs);

  switch (inst->binop.opcode) {
  case UIROpcode::Add: {
    // TODO: `compareTypes`
    if (lhs.lit_type->kind == TypeKind::Integer &&
        rhs.lit_type->kind == TypeKind::Integer) {
      lhs._int += rhs._int;
      return lhs;
    } else if (lhs.lit_type->kind == TypeKind::Float &&
               rhs.lit_type->kind == TypeKind::Float) {
      lhs._float += rhs._float;
      return lhs;
    }
    break;
  }
  case UIROpcode::Sub: {
    // TODO: `compareTypes`
    if (lhs.lit_type->kind == TypeKind::Integer &&
        rhs.lit_type->kind == TypeKind::Integer) {
      lhs._int -= rhs._int;
      return lhs;
    } else if (lhs.lit_type->kind == TypeKind::Float &&
               rhs.lit_type->kind == TypeKind::Float) {
      lhs._float -= rhs._float;
      return lhs;
    }
    break;
  }
  case UIROpcode::Mul: {
    // TODO: `compareTypes`
    if (lhs.lit_type->kind == TypeKind::Integer &&
        rhs.lit_type->kind == TypeKind::Integer) {
      lhs._int *= rhs._int;
      return lhs;
    } else if (lhs.lit_type->kind == TypeKind::Float &&
               rhs.lit_type->kind == TypeKind::Float) {
      lhs._float *= rhs._float;
      return lhs;
    }
    break;
  }
  case UIROpcode::Div: {
    // TODO: `compareTypes`
    if (lhs.lit_type->kind == TypeKind::Integer &&
        rhs.lit_type->kind == TypeKind::Integer) {
      lhs._int /= rhs._int;
      return lhs;
    } else if (lhs.lit_type->kind == TypeKind::Float &&
               rhs.lit_type->kind == TypeKind::Float) {
      lhs._float /= rhs._float;
      return lhs;
    }
    break;
  }
  case UIROpcode::Mod: {
    // TODO: `compareTypes`
    if (lhs.lit_type->kind == TypeKind::Integer &&
        rhs.lit_type->kind == TypeKind::Integer) {
      lhs._int %= rhs._int;
      return lhs;
    } else if (lhs.lit_type->kind == TypeKind::Float &&
               rhs.lit_type->kind == TypeKind::Float) {
      lhs._float = std::fmod(lhs._float, rhs._float);
      return lhs;
    }
    break;
  }
  case UIROpcode::Or: {
    // TODO: `compareTypes`
    if (lhs.lit_type->kind == TypeKind::Bool &&
        rhs.lit_type->kind == TypeKind::Bool) {
      lhs._bool |= rhs._bool;
      return lhs;
    } else if (lhs.lit_type->kind == TypeKind::Integer &&
               rhs.lit_type->kind == TypeKind::Integer) {
      lhs._int |= rhs._int;
      return lhs;
    }
    break;
  }
  case UIROpcode::Xor: {
    // TODO: `compareTypes`
    if (lhs.lit_type->kind == TypeKind::Bool &&
        rhs.lit_type->kind == TypeKind::Bool) {
      lhs._bool ^= rhs._bool;
      return lhs;
    } else if (lhs.lit_type->kind == TypeKind::Integer &&
               rhs.lit_type->kind == TypeKind::Integer) {
      lhs._int ^= rhs._int;
      return lhs;
    }
    break;
  }
  case UIROpcode::And: {
    // TODO: `compareTypes`
    if (lhs.lit_type->kind == TypeKind::Bool &&
        rhs.lit_type->kind == TypeKind::Bool) {
      lhs._bool &= rhs._bool;
      return lhs;
    } else if (lhs.lit_type->kind == TypeKind::Integer &&
               rhs.lit_type->kind == TypeKind::Integer) {
      lhs._int &= rhs._int;
      return lhs;
    }
    break;
  }
  case UIROpcode::LeftShift: {
    // TODO: `compareTypes`
    if (lhs.lit_type->kind == TypeKind::Integer &&
        rhs.lit_type->kind == TypeKind::Integer) {
      lhs._int <<= rhs._int;
      return lhs;
    }
    break;
  }
  case UIROpcode::RightShift: {
    // TODO: `compareTypes`
    if (lhs.lit_type->kind == TypeKind::Integer &&
        rhs.lit_type->kind == TypeKind::Integer) {
      lhs._int >>= rhs._int;
      return lhs;
    }
    break;
  }
  case UIROpcode::EqualTo:
  case UIROpcode::NotEqualTo: {
    // TODO: `compareTypes`
    UIRLiteral result = {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::Bool}),
        .kind = UIRLiteralKind::Typed,
    };

    if (lhs.lit_type->kind == TypeKind::Bool &&
        rhs.lit_type->kind == TypeKind::Bool) {
      result._bool = lhs._bool == rhs._bool;
    } else if (lhs.lit_type->kind == TypeKind::Integer &&
               rhs.lit_type->kind == TypeKind::Integer) {
      result._bool = lhs._int == rhs._int;
    } else if (lhs.lit_type->kind == TypeKind::Float &&
               rhs.lit_type->kind == TypeKind::Float) {
      result._bool = lhs._float == rhs._float;
    } else if (lhs.lit_type->kind == TypeKind::Pointer &&
               rhs.lit_type->kind == TypeKind::Pointer) {
      result._bool = lhs.pointer == rhs.pointer;
    } else if (lhs.lit_type->kind == TypeKind::Enum &&
               rhs.lit_type->kind == TypeKind::Enum) {
      result._bool = lhs._int == rhs._int;
    } else {
      break;
    }

    if (inst->binop.opcode == UIROpcode::NotEqualTo) {
      result._bool = !result._bool;
    }
    return result;
  }
  case UIROpcode::LessThen: {
    // TODO: `compareTypes`
    UIRLiteral result = {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::Bool}),
        .kind = UIRLiteralKind::Typed,
    };

    if (lhs.lit_type->kind == TypeKind::Integer &&
        rhs.lit_type->kind == TypeKind::Integer) {
      result._bool = lhs._int < rhs._int;
      return result;
    } else if (lhs.lit_type->kind == TypeKind::Float &&
               rhs.lit_type->kind == TypeKind::Float) {
      result._bool = lhs._float < rhs._float;
      return result;
    }
  }
  case UIROpcode::GreaterThen: {
    // TODO: `compareTypes`
    UIRLiteral result = {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::Bool}),
        .kind = UIRLiteralKind::Typed,
    };

    if (lhs.lit_type->kind == TypeKind::Integer &&
        rhs.lit_type->kind == TypeKind::Integer) {
      result._bool = lhs._int > rhs._int;
      return result;
    } else if (lhs.lit_type->kind == TypeKind::Float &&
               rhs.lit_type->kind == TypeKind::Float) {
      result._bool = lhs._float > rhs._float;
      return result;
    }
  }
  case UIROpcode::LessThenOrEqualTo: {
    // TODO: `compareTypes`
    UIRLiteral result = {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::Bool}),
        .kind = UIRLiteralKind::Typed,
    };

    if (lhs.lit_type->kind == TypeKind::Integer &&
        rhs.lit_type->kind == TypeKind::Integer) {
      result._bool = lhs._int <= rhs._int;
      return result;
    } else if (lhs.lit_type->kind == TypeKind::Float &&
               rhs.lit_type->kind == TypeKind::Float) {
      result._bool = lhs._float <= rhs._float;
      return result;
    }
  }
  case UIROpcode::GreaterThenOrEqualTo: {
    // TODO: `compareTypes`
    UIRLiteral result = {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::Bool}),
        .kind = UIRLiteralKind::Typed,
    };

    if (lhs.lit_type->kind == TypeKind::Integer &&
        rhs.lit_type->kind == TypeKind::Integer) {
      result._bool = lhs._int >= rhs._int;
      return result;
    } else if (lhs.lit_type->kind == TypeKind::Float &&
               rhs.lit_type->kind == TypeKind::Float) {
      result._bool = lhs._float >= rhs._float;
      return result;
    }
  }
  }

  std::cerr << "Opcode `" << (uint16_t)inst->binop.opcode
            << "` cannot operate on `" << lhs.lit_type << "` and `"
            << rhs.lit_type << "`\n";
  std::abort();
}
