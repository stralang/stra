#pragma once

#include "uir.hpp"

std::ostream &printModule(std::ostream &os, UIRContext *ctx, UIRModule *module);

std::ostream &printInst(std::ostream &os, UIRContext *ctx, UIRValue *inst,
                        std::string indent);
std::ostream &printBlock(std::ostream &os, UIRContext *ctx, UIRBlock *block,
                         std::string indent);
std::ostream &printScope(std::ostream &os, UIRContext *ctx, UIRScope *scope,
                         std::string indent);
