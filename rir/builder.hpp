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

  RIRValueId buildLocalVariable(RIRTypeId type);
  RIRValueId buildLoad(RIRValueId ptr, RIRTypeId result);
  RIRValueId buildStore(RIRValueId ptr, RIRValueId value);
  RIRValueId buildArg(RIRTypeId type);
  RIRValueId buildBinOp(RIROpcode opcode, RIRValueId lhs, RIRValueId rhs,
                        RIRTypeId result);
  RIRValueId buildUnaryOp(RIROpcode opcode, RIRValueId child, RIRTypeId result);
  RIRValueId buildCast(RIRValueId value, RIRTypeId result, bool bitcast);
  RIRValueId buildCall(RIRValueId callee, Slice<RIRValueId> arguments,
                       RIRTypeId result);
  RIRValueId buildIndex(RIRValueId ptr, RIRValueId index, RIRTypeId result);
  RIRValueId buildRange(RIRValueId ptr, RIRValueId offset, RIRValueId length,
                        RIRTypeId result_elem_type);
  RIRValueId buildFieldAt(RIRValueId ptr, RIRValueId index, RIRTypeId result);
  RIRValueId buildReturn(Option<RIRValueId> value);
  RIRValueId buildBranch(RIRBlockId dest);
  RIRValueId buildCondBranch(RIRValueId condition, RIRBlockId then,
                             RIRBlockId _else);
  RIRValueId buildSwitch(RIRValueId condition, RIRBlockId default_dest,
                         size_t case_count);
  void addCase(RIRValueId _switch, RIRValueId on_val, RIRBlockId dest,
               size_t _case);

  RIRValueId buildGlobalVariable(RIRTypeId type, Option<RIRConstant> value,
                                 String link_name);
  RIRValueId buildFunction(RIRTypeId type, bool undefined, String link_name);

  RIRValueId buildConstant(RIRTypeId type, RIRConstant constant);

  RIRBlockId appendBlock(RIRValueId function);

  void addDebugLocation(RIRValueId inst, SrcLoc location);
  void addDebugName(RIRValueId inst, String name);
};
