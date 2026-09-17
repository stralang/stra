#pragma once

#include "codegen.hpp"
#include "uir/literal.hpp"
#include "uir/uir.hpp"
#include "llvm-c/Types.h"
#include <llvm-c/Core.h>

// Base
LLVMValueRef getReference(CodeGenModule *codegen, UIRValue *value);
void gen(CodeGenModule *codegen, LLVMBuilderRef builder, UIRValue *inst);
void genDeclaration(CodeGenModule *codegen, UIRValue *inst);

// Conversion
LLVMTypeRef typeToLLVM(CodeGenModule *codegen, Type *type,
                       const char *name = nullptr);
LLVMValueRef literalToLLVM(CodeGenModule *codegen, UIRLiteral *literal);

// Operator
LLVMValueRef genMemberAccess(CodeGenModule *codegen, LLVMBuilderRef builder,
                             Node *node, Symbol *scope);
LLVMValueRef addrCastAs(CodeGenModule *codegen, LLVMBuilderRef builder,
                        Node *node, Symbol *scope);

LLVMValueRef genUnary(CodeGenModule *codegen, LLVMBuilderRef builder,
                      UIRValue *inst);
LLVMValueRef genBinary(CodeGenModule *codegen, LLVMBuilderRef builder,
                       UIRValue *inst);

LLVMValueRef genLookupPtr(CodeGenModule *codegen, LLVMBuilderRef builder,
                          UIRValue *inst);

// Function
void genFunctionBody(CodeGenModule *codegen, LLVMBuilderRef builder,
                     UIRValue *inst);
LLVMValueRef genCall(CodeGenModule *codegen, LLVMBuilderRef builder,
                     UIRValue *inst);

LLVMValueRef genCallBuiltin(CodeGenModule *codegen, LLVMBuilderRef builder,
                            Node *builtin_name, Value *callee,
                            Slice<LLVMValueRef> args);

// Helpers
void injectDefer(CodeGenModule *codegen, LLVMBuilderRef builder, Symbol *scope,
                 bool is_loop);

inline LLVMValueRef BuildAlloca(CodeGenModule *codegen, LLVMBuilderRef builder,
                                LLVMTypeRef ty, const char *name) {
  LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(builder);
  LLVMPositionBuilderAtEnd(builder, codegen->define_block);

  LLVMValueRef value = LLVMBuildAlloca(builder, ty, name);

  LLVMPositionBuilderAtEnd(builder, insert_block);
  return value;
}
