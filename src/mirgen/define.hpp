#pragma once

#include "mirgen.hpp"

MIRValue *gen(MIRGen *mirgen, Node *node, Symbol *scope);

MIRValue *genBuiltin(MIRGen *mirgen, String name);

MIRValue *valueToMIR(MIRGen *mirgen, Value *value);
