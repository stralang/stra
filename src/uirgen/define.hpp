#pragma once

#include "uirgen.hpp"
#include <sstream>

UIRValue *genComptime(UIRGen *uirgen, Node *node, Symbol *scope);

UIRValue *addr(UIRGen *uirgen, Node *node, Symbol *scope);
UIRValue *gen(UIRGen *uirgen, Node *node, Symbol *scope);
void injectDefer(UIRGen *uirgen, Symbol *scope, bool is_return);
void genDeclaration(UIRGen *uirgen, Node *node, Symbol *scope);

void genIf(UIRGen *uirgen, Node *node, Symbol *scope);
void genLoop(UIRGen *uirgen, Node *node, Symbol *scope);
void genSwitch(UIRGen *uirgen, Node *node, Symbol *scope);

UIRValue *genAssignment(UIRGen *uirgen, Node *node, Symbol *scope);
UIRValue *genUnary(UIRGen *uirgen, Node *node, Symbol *scope);
UIRValue *genBinary(UIRGen *uirgen, Node *node, Symbol *scope);
UIRValue *addrMemberAccess(UIRGen *uirgen, Node *node, Symbol *scope);

UIRValue *genStruct(UIRGen *uirgen, Node *node, Symbol *scope);
UIRValue *genEnum(UIRGen *uirgen, Node *node, Symbol *scope);
UIRValue *genUnion(UIRGen *uirgen, Node *node, Symbol *scope);
UIRValue *genNamespace(UIRGen *uirgen, Node *node, Symbol *scope);

UIRValue *genBuiltin(UIRGen *uirgen, String name);

UIRValue *valueToUIR(UIRGen *uirgen, Value *value);

#define expect(ok, srcloc, msg)                                                \
  if (!(ok)) {                                                                 \
    std::ostringstream os;                                                     \
    os << msg;                                                                 \
    std::string cpp_str = os.str();                                            \
    String m = {(uint8_t *)cpp_str.data(), cpp_str.size()};                    \
    uirgen->error_func(srcloc, m);                                             \
    uirgen->error_count += 1;                                                  \
  }
