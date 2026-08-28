#pragma once

#include "mirgen.hpp"

MIRValue *addr(MIRGen *mirgen, Node *node, Symbol *scope);
MIRValue *gen(MIRGen *mirgen, Node *node, Symbol *scope);
void genDeclaration(MIRGen *mirgen, Node *node, Symbol *scope);

MIRValue *genAssignment(MIRGen *mirgen, Node *node, Symbol *scope);
MIRValue *genUnary(MIRGen *mirgen, Node *node, Symbol *scope);
MIRValue *genBinary(MIRGen *mirgen, Node *node, Symbol *scope);
MIRValue *addrMemberAccess(MIRGen *mirgen, Node *node, Symbol *scope);

MIRValue *genStruct(MIRGen *mirgen, Node *node, Symbol *scope);
MIRValue *genEnum(MIRGen *mirgen, Node *node, Symbol *scope);
MIRValue *genUnion(MIRGen *mirgen, Node *node, Symbol *scope);
MIRValue *genNamespace(MIRGen *mirgen, Node *node, Symbol *scope);

MIRValue *genBuiltin(MIRGen *mirgen, String name);

MIRValue *valueToMIR(MIRGen *mirgen, Value *value);
