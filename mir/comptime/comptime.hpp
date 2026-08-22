#pragma once

#include "literal.hpp"
#include "mir.hpp"

struct MIRComptime {
  MIRLiteral execute(MIRModule *module, MIRValue *inst);
};
