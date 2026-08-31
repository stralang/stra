#pragma once

#include "comptime.hpp"
#include "literal.hpp"
#include "mir.hpp"

void executeProgram(MIRComptime *state, MIRModule *module,
                    MIRBlock *entrypoint);

MIRLiteral execute(MIRComptime *state, MIRModule *module, MIRValue *inst);
MIRLiteral executeBinary(MIRComptime *state, MIRModule *module,
                         ComptimeStackFrame *frame, MIRValue *inst);

MIRLiteral executeGlobalVariable(MIRComptime *state, MIRModule *module,
                                 ComptimeStackFrame *frame, MIRValue *inst);
