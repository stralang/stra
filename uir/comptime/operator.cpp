#include "../analysis/analysis.hpp"
#include "../literal.hpp"
#include "../uir.hpp"
#include "define.hpp"
#include <cmath>
#include <iostream>

UIRLiteral executeBinary(UIRComptime *state, UIRModule *module,
                         ComptimeStackFrame *frame, UIRValue *inst) {
  UIRLiteral lhs = state->getValue(frame, module, inst->binop.lhs);
  UIRLiteral rhs = state->getValue(frame, module, inst->binop.rhs);

  RIRType *lhs_type = state->analyser->rir_ctx->getType(lhs.lit_type);
  RIRType *rhs_type = state->analyser->rir_ctx->getType(rhs.lit_type);

  switch (inst->binop.opcode) {
  case UIROpcode::Add: {
    // TODO: `compareTypes`
    if (lhs_type->kind == RIRTypeKind::Integer &&
        rhs_type->kind == RIRTypeKind::Integer) {
      lhs._int += rhs._int;
      return lhs;
    } else if (lhs_type->kind == RIRTypeKind::Float &&
               rhs_type->kind == RIRTypeKind::Float) {
      lhs._float += rhs._float;
      return lhs;
    }
    break;
  }
  case UIROpcode::Sub: {
    // TODO: `compareTypes`
    if (lhs_type->kind == RIRTypeKind::Integer &&
        rhs_type->kind == RIRTypeKind::Integer) {
      lhs._int -= rhs._int;
      return lhs;
    } else if (lhs_type->kind == RIRTypeKind::Float &&
               rhs_type->kind == RIRTypeKind::Float) {
      lhs._float -= rhs._float;
      return lhs;
    }
    break;
  }
  case UIROpcode::Mul: {
    // TODO: `compareTypes`
    if (lhs_type->kind == RIRTypeKind::Integer &&
        rhs_type->kind == RIRTypeKind::Integer) {
      lhs._int *= rhs._int;
      return lhs;
    } else if (lhs_type->kind == RIRTypeKind::Float &&
               rhs_type->kind == RIRTypeKind::Float) {
      lhs._float *= rhs._float;
      return lhs;
    }
    break;
  }
  case UIROpcode::Div: {
    // TODO: `compareTypes`
    if (lhs_type->kind == RIRTypeKind::Integer &&
        rhs_type->kind == RIRTypeKind::Integer) {
      lhs._int /= rhs._int;
      return lhs;
    } else if (lhs_type->kind == RIRTypeKind::Float &&
               rhs_type->kind == RIRTypeKind::Float) {
      lhs._float /= rhs._float;
      return lhs;
    }
    break;
  }
  case UIROpcode::Mod: {
    // TODO: `compareTypes`
    if (lhs_type->kind == RIRTypeKind::Integer &&
        rhs_type->kind == RIRTypeKind::Integer) {
      lhs._int %= rhs._int;
      return lhs;
    } else if (lhs_type->kind == RIRTypeKind::Float &&
               rhs_type->kind == RIRTypeKind::Float) {
      lhs._float = std::fmod(lhs._float, rhs._float);
      return lhs;
    }
    break;
  }
  case UIROpcode::Or: {
    // TODO: `compareTypes`
    if (lhs_type->kind == RIRTypeKind::Bool &&
        rhs_type->kind == RIRTypeKind::Bool) {
      lhs._bool |= rhs._bool;
      return lhs;
    } else if (lhs_type->kind == RIRTypeKind::Integer &&
               rhs_type->kind == RIRTypeKind::Integer) {
      lhs._int |= rhs._int;
      return lhs;
    }
    break;
  }
  case UIROpcode::Xor: {
    // TODO: `compareTypes`
    if (lhs_type->kind == RIRTypeKind::Bool &&
        rhs_type->kind == RIRTypeKind::Bool) {
      lhs._bool ^= rhs._bool;
      return lhs;
    } else if (lhs_type->kind == RIRTypeKind::Integer &&
               rhs_type->kind == RIRTypeKind::Integer) {
      lhs._int ^= rhs._int;
      return lhs;
    }
    break;
  }
  case UIROpcode::And: {
    // TODO: `compareTypes`
    if (lhs_type->kind == RIRTypeKind::Bool &&
        rhs_type->kind == RIRTypeKind::Bool) {
      lhs._bool &= rhs._bool;
      return lhs;
    } else if (lhs_type->kind == RIRTypeKind::Integer &&
               rhs_type->kind == RIRTypeKind::Integer) {
      lhs._int &= rhs._int;
      return lhs;
    }
    break;
  }
  case UIROpcode::LeftShift: {
    // TODO: `compareTypes`
    if (lhs_type->kind == RIRTypeKind::Integer &&
        rhs_type->kind == RIRTypeKind::Integer) {
      lhs._int <<= rhs._int;
      return lhs;
    }
    break;
  }
  case UIROpcode::RightShift: {
    // TODO: `compareTypes`
    if (lhs_type->kind == RIRTypeKind::Integer &&
        rhs_type->kind == RIRTypeKind::Integer) {
      lhs._int >>= rhs._int;
      return lhs;
    }
    break;
  }
  case UIROpcode::EqualTo:
  case UIROpcode::NotEqualTo: {
    // TODO: `compareTypes`
    UIRLiteral result = {
        .lit_type =
            state->analyser->rir_ctx->types.push({.kind = RIRTypeKind::Bool}),
        .kind = UIRLiteralKind::Typed,
    };

    if (lhs_type->kind == RIRTypeKind::Bool &&
        rhs_type->kind == RIRTypeKind::Bool) {
      result._bool = lhs._bool == rhs._bool;
    } else if (lhs_type->kind == RIRTypeKind::Integer &&
               rhs_type->kind == RIRTypeKind::Integer) {
      result._bool = lhs._int == rhs._int;
    } else if (lhs_type->kind == RIRTypeKind::Float &&
               rhs_type->kind == RIRTypeKind::Float) {
      result._bool = lhs._float == rhs._float;
    } else if (lhs_type->kind == RIRTypeKind::Pointer &&
               rhs_type->kind == RIRTypeKind::Pointer) {
      result._bool = lhs.pointer == rhs.pointer;
    } else if (lhs_type->kind == RIRTypeKind::Enum &&
               rhs_type->kind == RIRTypeKind::Enum) {
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
        .lit_type =
            state->analyser->rir_ctx->types.push({.kind = RIRTypeKind::Bool}),
        .kind = UIRLiteralKind::Typed,
    };

    if (lhs_type->kind == RIRTypeKind::Integer &&
        rhs_type->kind == RIRTypeKind::Integer) {
      result._bool = lhs._int < rhs._int;
      return result;
    } else if (lhs_type->kind == RIRTypeKind::Float &&
               rhs_type->kind == RIRTypeKind::Float) {
      result._bool = lhs._float < rhs._float;
      return result;
    }
  }
  case UIROpcode::GreaterThen: {
    // TODO: `compareTypes`
    UIRLiteral result = {
        .lit_type =
            state->analyser->rir_ctx->types.push({.kind = RIRTypeKind::Bool}),
        .kind = UIRLiteralKind::Typed,
    };

    if (lhs_type->kind == RIRTypeKind::Integer &&
        rhs_type->kind == RIRTypeKind::Integer) {
      result._bool = lhs._int > rhs._int;
      return result;
    } else if (lhs_type->kind == RIRTypeKind::Float &&
               rhs_type->kind == RIRTypeKind::Float) {
      result._bool = lhs._float > rhs._float;
      return result;
    }
  }
  case UIROpcode::LessThenOrEqualTo: {
    // TODO: `compareTypes`
    UIRLiteral result = {
        .lit_type =
            state->analyser->rir_ctx->types.push({.kind = RIRTypeKind::Bool}),
        .kind = UIRLiteralKind::Typed,
    };

    if (lhs_type->kind == RIRTypeKind::Integer &&
        rhs_type->kind == RIRTypeKind::Integer) {
      result._bool = lhs._int <= rhs._int;
      return result;
    } else if (lhs_type->kind == RIRTypeKind::Float &&
               rhs_type->kind == RIRTypeKind::Float) {
      result._bool = lhs._float <= rhs._float;
      return result;
    }
  }
  case UIROpcode::GreaterThenOrEqualTo: {
    // TODO: `compareTypes`
    UIRLiteral result = {
        .lit_type =
            state->analyser->rir_ctx->types.push({.kind = RIRTypeKind::Bool}),
        .kind = UIRLiteralKind::Typed,
    };

    if (lhs_type->kind == RIRTypeKind::Integer &&
        rhs_type->kind == RIRTypeKind::Integer) {
      result._bool = lhs._int >= rhs._int;
      return result;
    } else if (lhs_type->kind == RIRTypeKind::Float &&
               rhs_type->kind == RIRTypeKind::Float) {
      result._bool = lhs._float >= rhs._float;
      return result;
    }
  }
  }

  std::cerr << "Opcode `" << (uint16_t)inst->binop.opcode
            << "` cannot operate on `" << lhs_type << "` and `" << rhs_type
            << "`\n";
  std::abort();
}
