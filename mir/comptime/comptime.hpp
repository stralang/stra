#pragma once

#include "containers.hpp"
#include "literal.hpp"
#include "mir.hpp"

struct MIRComptime {
  DynamicArena *arena;

  MIRLiteral execute(MIRModule *module, MIRValue *inst);
};
