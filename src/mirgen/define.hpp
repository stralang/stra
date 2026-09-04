#pragma once

#include "mirgen.hpp"

MIRValueId addr(MIRGen *mirgen, Node *node, Symbol *scope);
MIRValueId gen(MIRGen *mirgen, Node *node, Symbol *scope);
void genDeclaration(MIRGen *mirgen, Node *node, Symbol *scope);

MIRValueId genAssignment(MIRGen *mirgen, Node *node, Symbol *scope);
MIRValueId genUnary(MIRGen *mirgen, Node *node, Symbol *scope);
MIRValueId genBinary(MIRGen *mirgen, Node *node, Symbol *scope);
MIRValueId addrMemberAccess(MIRGen *mirgen, Node *node, Symbol *scope);

MIRValueId genStruct(MIRGen *mirgen, Node *node, Symbol *scope);
MIRValueId genEnum(MIRGen *mirgen, Node *node, Symbol *scope);
MIRValueId genUnion(MIRGen *mirgen, Node *node, Symbol *scope);
MIRValueId genNamespace(MIRGen *mirgen, Node *node, Symbol *scope);

Option<MIRValueId> genBuiltin(MIRGen *mirgen, String name);

MIRValueId valueToMIR(MIRGen *mirgen, Value *value);
