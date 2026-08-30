#include "define.hpp"
#include "literal.hpp"
#include "mir.hpp"
#include <cmath>
#include <iostream>

MIRLiteral executeBinary(MIRComptime *state, MIRModule *module,
                         ComptimeStackFrame *frame, MIRValue *inst) {
  MIRLiteral lhs = *frame->get(inst->binop.lhs);
  MIRLiteral rhs = *frame->get(inst->binop.rhs);

  switch (inst->binop.opcode) {
  case MIROpcode::Add: {
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
  case MIROpcode::Sub: {
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
  case MIROpcode::Mul: {
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
  case MIROpcode::Div: {
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
  case MIROpcode::Mod: {
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
  case MIROpcode::Or: {
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
  case MIROpcode::Xor: {
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
  case MIROpcode::And: {
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
  case MIROpcode::LeftShift: {
    // TODO: `compareTypes`
    if (lhs.lit_type->kind == TypeKind::Integer &&
        rhs.lit_type->kind == TypeKind::Integer) {
      lhs._int <<= rhs._int;
      return lhs;
    }
    break;
  }
  case MIROpcode::RightShift: {
    // TODO: `compareTypes`
    if (lhs.lit_type->kind == TypeKind::Integer &&
        rhs.lit_type->kind == TypeKind::Integer) {
      lhs._int >>= rhs._int;
      return lhs;
    }
    break;
  }
  case MIROpcode::EqualTo:
  case MIROpcode::NotEqualTo: {
    // TODO: `compareTypes`
    MIRLiteral result = {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::Bool}),
        .kind = MIRLiteralKind::Typed,
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

    if (inst->binop.opcode == MIROpcode::NotEqualTo) {
      result._bool = !result._bool;
    }
    return result;
  }
  case MIROpcode::LessThen: {
    // TODO: `compareTypes`
    MIRLiteral result = {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::Bool}),
        .kind = MIRLiteralKind::Typed,
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
  case MIROpcode::GreaterThen: {
    // TODO: `compareTypes`
    MIRLiteral result = {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::Bool}),
        .kind = MIRLiteralKind::Typed,
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
  case MIROpcode::LessThenOrEqualTo: {
    // TODO: `compareTypes`
    MIRLiteral result = {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::Bool}),
        .kind = MIRLiteralKind::Typed,
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
  case MIROpcode::GreaterThenOrEqualTo: {
    // TODO: `compareTypes`
    MIRLiteral result = {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::Bool}),
        .kind = MIRLiteralKind::Typed,
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
