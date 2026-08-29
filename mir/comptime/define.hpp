#pragma once

#include "comptime.hpp"

void executeProgram(MIRComptime *state, MIRModule *module,
                    MIRBlock *entrypoint);

MIRLiteral execute(MIRComptime *state, MIRModule *module, MIRValue *inst);
MIRLiteral executeBinary(MIRComptime *state, MIRModule *module,
                         ComptimeStackFrame *frame, MIRValue *inst);
