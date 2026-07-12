#pragma once

#include "codegen.hpp"
#include "llvm-c/Types.h"
#include <llvm-c/Core.h>

// Base
LLVMValueRef gen(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                 Symbol *scope);
LLVMValueRef addr(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                  Symbol *scope);

// Conversion
LLVMTypeRef typeToLLVM(CodeGenModule *codegen, Type *type,
                       const char *name = nullptr);
LLVMValueRef valueToLLVM(CodeGenModule *codegen, Value *value);

// Operator
LLVMValueRef genMemberAccess(CodeGenModule *codegen, LLVMBuilderRef builder,
                             Node *node, Symbol *scope);
LLVMValueRef addrCastAs(CodeGenModule *codegen, LLVMBuilderRef builder,
                        Node *node, Symbol *scope);
LLVMValueRef genAssignment(CodeGenModule *codegen, LLVMBuilderRef builder,
                           Node *node, Symbol *scope);
LLVMValueRef genUnary(CodeGenModule *codegen, LLVMBuilderRef builder,
                      Node *node, Symbol *scope);
LLVMValueRef genBinary(CodeGenModule *codegen, LLVMBuilderRef builder,
                       Node *node, Symbol *scope);

// Function
void genFunctionBody(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                     Symbol *scope, LLVMTypeRef fn_type, LLVMValueRef func);
LLVMValueRef genCall(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                     Symbol *scope);

LLVMValueRef genCallBuiltin(CodeGenModule *codegen, LLVMBuilderRef builder,
                            Value *callee, Slice<LLVMValueRef> args);

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
