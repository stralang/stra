#pragma once

#include "comptime.hpp"
#include "literal.hpp"
#include "uir.hpp"

void executeProgram(UIRComptime *state, UIRModule *module,
                    UIRBlock *entrypoint);

UIRLiteral execute(UIRComptime *state, UIRModule *module, UIRValue *inst);
UIRLiteral executeBinary(UIRComptime *state, UIRModule *module,
                         ComptimeStackFrame *frame, UIRValue *inst);
