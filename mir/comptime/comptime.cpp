#include "comptime.hpp"
#include "literal.hpp"
#include "mir.hpp"
#include <cstdlib>
#include <iostream>

MIRLiteral execute(MIRComptime *state, MIRModule *module, MIRValue *inst) {
  switch (inst->kind) {
  case MIRValueKind::Literal: {
    return inst->literal;
  }
  }

  std::cerr << "TODO: Implement compile-time execution of `"
            << (uint16_t)inst->kind << "`\n";
  std::abort();
}

MIRLiteral MIRComptime::execute(MIRModule *module, MIRValue *inst) {
  return ::execute(this, module, inst);
}
