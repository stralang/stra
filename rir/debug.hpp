#pragma once

#include "rir.hpp"
#include <ostream>

std::ostream &printTypes(std::ostream &os, RIRContext *ctx);
std::ostream &printModule(std::ostream &os, RIRContext *ctx, RIRModule *module);

std::ostream &printInst(std::ostream &os, RIRContext *ctx, RIRValue *inst);
std::ostream &printBlock(std::ostream &os, RIRContext *ctx, RIRBlock *block);
std::ostream &printType(std::ostream &os, RIRContext *ctx, RIRType *type);
