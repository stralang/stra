#include "../analysis/analysis.hpp"
#include "../literal.hpp"
#include "../uir.hpp"
#include "define.hpp"
#include "rir/rir.hpp"
#include "rir/type.hpp"
#include <cmath>
#include <iostream>

UIRLiteral executeBinary(UIRComptime *state, UIRModule *module,
                         ComptimeStackFrame *frame, UIRValue *inst) {
  UIRLiteral lhs = state->getValue(frame, module, inst->binop.lhs);
  UIRLiteral rhs = state->getValue(frame, module, inst->binop.rhs);

  // Get types
  RIRType *lhs_type = nullptr;
  RIRType *rhs_type = nullptr;
  if (lhs.lit_type.isSome()) {
    lhs_type = state->analyser->rir_ctx->getType(lhs.lit_type.get());
  }
  if (rhs.lit_type.isSome()) {
    rhs_type = state->analyser->rir_ctx->getType(rhs.lit_type.get());
  }

  // Execute
  switch (inst->binop.opcode) {
  case UIROpcode::Add: {
    // TODO: `compareTypes`
    if (lhs.data.kind == UIRRawDataKind::Int &&
        rhs.data.kind == UIRRawDataKind::Int) {
      lhs.data._int += rhs.data._int;
      return lhs;
    } else if (lhs.data.kind == UIRRawDataKind::Float &&
               rhs.data.kind == UIRRawDataKind::Float) {
      lhs.data._float += rhs.data._float;
      return lhs;
    }
    break;
  }
  case UIROpcode::Sub: {
    // TODO: `compareTypes`
    if (lhs.data.kind == UIRRawDataKind::Int &&
        rhs.data.kind == UIRRawDataKind::Int) {
      lhs.data._int -= rhs.data._int;
      return lhs;
    } else if (lhs.data.kind == UIRRawDataKind::Float &&
               rhs.data.kind == UIRRawDataKind::Float) {
      lhs.data._float -= rhs.data._float;
      return lhs;
    }
    break;
  }
  case UIROpcode::Mul: {
    // TODO: `compareTypes`
    if (lhs.data.kind == UIRRawDataKind::Int &&
        rhs.data.kind == UIRRawDataKind::Int) {
      lhs.data._int *= rhs.data._int;
      return lhs;
    } else if (lhs.data.kind == UIRRawDataKind::Float &&
               rhs.data.kind == UIRRawDataKind::Float) {
      lhs.data._float *= rhs.data._float;
      return lhs;
    }
    break;
  }
  case UIROpcode::Div: {
    // TODO: `compareTypes`
    if (lhs.data.kind == UIRRawDataKind::Int &&
        rhs.data.kind == UIRRawDataKind::Int) {
      lhs.data._int /= rhs.data._int;
      return lhs;
    } else if (lhs.data.kind == UIRRawDataKind::Float &&
               rhs.data.kind == UIRRawDataKind::Float) {
      lhs.data._float /= rhs.data._float;
      return lhs;
    }
    break;
  }
  case UIROpcode::Mod: {
    // TODO: `compareTypes`
    if (lhs.data.kind == UIRRawDataKind::Int &&
        rhs.data.kind == UIRRawDataKind::Int) {
      lhs.data._int %= rhs.data._int;
      return lhs;
    } else if (lhs.data.kind == UIRRawDataKind::Float &&
               rhs.data.kind == UIRRawDataKind::Float) {
      lhs.data._float = std::fmod(lhs.data._float, rhs.data._float);
      return lhs;
    }
    break;
  }
  case UIROpcode::Or: {
    // TODO: `compareTypes`
    if (lhs.data.kind == UIRRawDataKind::Bool &&
        rhs.data.kind == UIRRawDataKind::Bool) {
      lhs.data._bool |= rhs.data._bool;
      return lhs;
    } else if (lhs.data.kind == UIRRawDataKind::Int &&
               rhs.data.kind == UIRRawDataKind::Int) {
      lhs.data._int |= rhs.data._int;
      return lhs;
    }
    break;
  }
  case UIROpcode::Xor: {
    // TODO: `compareTypes`
    if (lhs.data.kind == UIRRawDataKind::Bool &&
        rhs.data.kind == UIRRawDataKind::Bool) {
      lhs.data._bool ^= rhs.data._bool;
      return lhs;
    } else if (lhs.data.kind == UIRRawDataKind::Int &&
               rhs.data.kind == UIRRawDataKind::Int) {
      lhs.data._int ^= rhs.data._int;
      return lhs;
    }
    break;
  }
  case UIROpcode::And: {
    // TODO: `compareTypes`
    if (lhs.data.kind == UIRRawDataKind::Bool &&
        rhs.data.kind == UIRRawDataKind::Bool) {
      lhs.data._bool &= rhs.data._bool;
      return lhs;
    } else if (lhs.data.kind == UIRRawDataKind::Int &&
               rhs.data.kind == UIRRawDataKind::Int) {
      lhs.data._int &= rhs.data._int;
      return lhs;
    }
    break;
  }
  case UIROpcode::LeftShift: {
    // TODO: `compareTypes`
    if (lhs.data.kind == UIRRawDataKind::Int &&
        rhs.data.kind == UIRRawDataKind::Int) {
      lhs.data._int <<= rhs.data._int;
      return lhs;
    }
    break;
  }
  case UIROpcode::RightShift: {
    // TODO: `compareTypes`
    if (lhs.data.kind == UIRRawDataKind::Int &&
        rhs.data.kind == UIRRawDataKind::Int) {
      lhs.data._int >>= rhs.data._int;
      return lhs;
    }
    break;
  }
  case UIROpcode::EqualTo:
  case UIROpcode::NotEqualTo: {
    // TODO: `compareTypes`
    UIRLiteral result = {.data = {.kind = UIRRawDataKind::Bool}};

    if (lhs.data.kind == UIRRawDataKind::Bool &&
        rhs.data.kind == UIRRawDataKind::Bool) {
      result.data._bool = lhs.data._bool == rhs.data._bool;
    } else if (lhs.data.kind == UIRRawDataKind::Int &&
               rhs.data.kind == UIRRawDataKind::Int) {
      result.data._bool = lhs.data._int == rhs.data._int;
    } else if (lhs.data.kind == UIRRawDataKind::Float &&
               rhs.data.kind == UIRRawDataKind::Float) {
      result.data._bool = lhs.data._float == rhs.data._float;
    } else if (lhs.data.kind == UIRRawDataKind::Pointer &&
               rhs.data.kind == UIRRawDataKind::Pointer) {
      if (lhs.data.ptr.kind == UIRPlaceKind::Inst) {
        RIRValueId lhs_inst = lhs.data.ptr.inst;
        RIRValueId rhs_inst = rhs.data.ptr.inst;
        result.data._bool = lhs_inst.module == rhs_inst.module &&
                            lhs_inst.local == rhs_inst.local;
      } else if (lhs.data.ptr.kind == UIRPlaceKind::Raw) {
        result.data._bool = lhs.data.ptr.data == rhs.data.ptr.data;
      }
    } else {
      break;
    }

    if (inst->binop.opcode == UIROpcode::NotEqualTo) {
      result.data._bool = !result.data._bool;
    }
    return result;
  }
  case UIROpcode::LessThen: {
    // TODO: `compareTypes`
    UIRLiteral result = {.data = {.kind = UIRRawDataKind::Bool}};

    if (lhs.data.kind == UIRRawDataKind::Int &&
        rhs.data.kind == UIRRawDataKind::Int) {
      result.data._bool = lhs.data._int < rhs.data._int;
      return result;
    } else if (lhs.data.kind == UIRRawDataKind::Float &&
               rhs.data.kind == UIRRawDataKind::Float) {
      result.data._bool = lhs.data._float < rhs.data._float;
      return result;
    }
  }
  case UIROpcode::GreaterThen: {
    // TODO: `compareTypes`
    UIRLiteral result = {.data = {.kind = UIRRawDataKind::Bool}};

    if (lhs.data.kind == UIRRawDataKind::Int &&
        rhs.data.kind == UIRRawDataKind::Int) {
      result.data._bool = lhs.data._int > rhs.data._int;
      return result;
    } else if (lhs.data.kind == UIRRawDataKind::Float &&
               rhs.data.kind == UIRRawDataKind::Float) {
      result.data._bool = lhs.data._float > rhs.data._float;
      return result;
    }
  }
  case UIROpcode::LessThenOrEqualTo: {
    // TODO: `compareTypes`
    UIRLiteral result = {.data = {.kind = UIRRawDataKind::Bool}};

    if (lhs.data.kind == UIRRawDataKind::Int &&
        rhs.data.kind == UIRRawDataKind::Int) {
      result.data._bool = lhs.data._int <= rhs.data._int;
      return result;
    } else if (lhs.data.kind == UIRRawDataKind::Float &&
               rhs.data.kind == UIRRawDataKind::Float) {
      result.data._bool = lhs.data._float <= rhs.data._float;
      return result;
    }
  }
  case UIROpcode::GreaterThenOrEqualTo: {
    // TODO: `compareTypes`
    UIRLiteral result = {.data = {.kind = UIRRawDataKind::Bool}};

    if (lhs.data.kind == UIRRawDataKind::Int &&
        rhs.data.kind == UIRRawDataKind::Int) {
      result.data._bool = lhs.data._int >= rhs.data._int;
      return result;
    } else if (lhs.data.kind == UIRRawDataKind::Float &&
               rhs.data.kind == UIRRawDataKind::Float) {
      result.data._bool = lhs.data._float >= rhs.data._float;
      return result;
    }
  }
  }

  std::cerr << "Opcode `" << (uint16_t)inst->binop.opcode
            << "` cannot operate on `" << lhs_type << "` and `" << rhs_type
            << "`\n";
  std::abort();
}
