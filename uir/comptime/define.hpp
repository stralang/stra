#pragma once

#include "../literal.hpp"
#include "../uir.hpp"
#include "comptime.hpp"

void executeProgram(UIRComptime *state, UIRModule *module,
                    UIRBlock *entrypoint);

UIRLiteral execute(UIRComptime *state, UIRModule *module, UIRValue *inst);
UIRLiteral executeBinary(UIRComptime *state, UIRModule *module,
                         ComptimeStackFrame *frame, UIRValue *inst);
