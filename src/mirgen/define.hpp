#pragma once

#include "mirgen.hpp"

MIRValue *gen(MIRGen *mirgen, Node *node, Symbol *scope);

MIRValue *valueToMIR(MIRGen *mirgen, Value *value);
