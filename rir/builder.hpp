#pragma once

#include "rir.hpp"
#include "rir/type.hpp"
#include "srcloc.hpp"

struct RIRBuilder {
  Option<RIRBlock *> block;
  RIRModule *module;
  RIRContext *ctx;

  RIRValueId build(RIRValue inst);
  RIRValueId buildDeclare(RIRValue inst);
  RIRValueId buildInst(RIRValue inst);

  RIRValueId buildLocalVariable(RIRTypeId type, RIRTypeId result);
  RIRValueId buildLoad(RIRValueId ptr, RIRTypeId result);
  RIRValueId buildStore(RIRValueId ptr, RIRValueId value, RIRTypeId result);
  RIRValueId buildArg(RIRTypeId result);
  RIRValueId buildBinOp(RIROpcode opcode, RIRValueId lhs, RIRValueId rhs,
                        RIRTypeId result);
  RIRValueId buildUnaryOp(RIROpcode opcode, RIRValueId child, RIRTypeId result);
  RIRValueId buildCall(RIRValueId callee, Slice<RIRValueId> arguments,
                       RIRTypeId result);
  RIRValueId buildGEP(RIRValueId ptr, RIRValueId index, RIRTypeId result);
  RIRValueId buildReturn(Option<RIRValueId> value);
  RIRValueId buildBranch(RIRBlockId dest);
  RIRValueId buildCondBranch(RIRValueId condition, RIRBlockId then,
                             RIRBlockId _else);
  RIRValueId buildSwitch(RIRValueId condition, RIRBlockId default_dest,
                         size_t case_count);
  void addCase(RIRValueId _switch, RIRValueId on_val, RIRBlockId dest,
               size_t _case);

  RIRValueId buildGlobalVariable(RIRTypeId type, Option<RIRValueId> value,
                                 RIRTypeId result);
  RIRValueId buildFunction(RIRTypeId type, bool undefined);

  RIRValueId buildConstant(RIRTypeId type, void *constant);

  void addDebugLocation(RIRValueId inst, SrcLoc location);
  void addDebugName(RIRValueId inst, String name);
};
