#pragma once

#include "../literal.hpp"
#include "../uir.hpp"
#include "comptime.hpp"

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
