#pragma once

#include "../literal.hpp"
#include "../uir.hpp"
#include "common/debug.hpp"
#include "comptime.hpp"

#define ctexpect(ok, srcloc, msg)                                              \
  if (!(ok)) {                                                                 \
    std::cerr << srcloc << " " << msg << "\n";                                 \
    std::abort();                                                              \
  }

void executeProgram(UIRComptime *state, UIRBlockId entrypoint_id);

void execute(UIRComptime *state, UIRValue *inst);
UIRLiteral **executeGetReturn(UIRComptime *state, UIRValue *inst);
UIRLiteral executeBinary(UIRComptime *state, ComptimeStackFrame *frame,
                         UIRValue *inst);

Option<UIRLiteral> executeLookupPtr(UIRComptime *state,
                                    ComptimeStackFrame *frame, UIRValue *inst);
Option<UIRLiteral> executeLookupValue(UIRComptime *state,
                                      ComptimeStackFrame *frame,
                                      UIRValue *inst);

void executeBuiltinCall(UIRComptime *state, UIRValue *call_inst,
                        UIRValue *func_inst);

void computeStructType(UIRComptime *state, UIRValue *inst,
                       RIRTypeId struct_type_id);
void computeEnumType(UIRComptime *state, UIRValue *inst,
                     RIRTypeId enum_type_id);
void computeUnionType(UIRComptime *state, UIRValue *inst,
                      RIRTypeId union_type_id);
