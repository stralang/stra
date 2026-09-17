#pragma once

#include "rir.hpp"
#include "srcloc.hpp"

struct RIRBuilder {
  Option<RIRBlock *> block;
  RIRModule *module;

  RIRValueId build(RIRValue inst);
  RIRValueId buildDeclare(RIRValue inst);
  RIRValueId buildInst(RIRValue inst);

  RIRValueId buildLocalVariable(RIRTypeId type);
  RIRValueId buildLoad(RIRValueId ptr);
  RIRValueId buildStore(RIRValueId ptr, RIRValueId value);

  void addDebugLocation(RIRValueId inst, SrcLoc location);
  void addDebugName(RIRValueId inst, String name);
};
