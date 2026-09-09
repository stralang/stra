#pragma once

#include "mirgen.hpp"
#include <sstream>

MIRValue *genComptime(MIRGen *mirgen, Node *node, Symbol *scope);

MIRValue *addr(MIRGen *mirgen, Node *node, Symbol *scope);
MIRValue *gen(MIRGen *mirgen, Node *node, Symbol *scope);
void injectDefer(MIRGen *mirgen, Symbol *scope, bool is_return);
void genDeclaration(MIRGen *mirgen, Node *node, Symbol *scope);

void genIf(MIRGen *mirgen, Node *node, Symbol *scope);
void genLoop(MIRGen *mirgen, Node *node, Symbol *scope);
void genSwitch(MIRGen *mirgen, Node *node, Symbol *scope);

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

#define expect(ok, srcloc, msg)                                                \
  if (!(ok)) {                                                                 \
    std::ostringstream os;                                                     \
    os << msg;                                                                 \
    std::string cpp_str = os.str();                                            \
    String m = {(uint8_t *)cpp_str.data(), cpp_str.size()};                    \
    mirgen->error_func(srcloc, m);                                             \
    mirgen->error_count += 1;                                                  \
  }
