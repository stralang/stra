#pragma once

#include "codegen.hpp"
#include "literal.hpp"
#include "mir.hpp"
#include "llvm-c/Types.h"
#include <llvm-c/Core.h>

// Base
LLVMValueRef getReference(CodeGenModule *codegen, MIRValueId value);
void gen(CodeGenModule *codegen, LLVMBuilderRef builder, MIRValue *inst);
void genDeclaration(CodeGenModule *codegen, MIRValue *inst);

// Conversion
LLVMTypeRef typeToLLVM(CodeGenModule *codegen, Type *type,
                       const char *name = nullptr);
LLVMValueRef literalToLLVM(CodeGenModule *codegen, MIRLiteral *literal);

// Operator
LLVMValueRef genMemberAccess(CodeGenModule *codegen, LLVMBuilderRef builder,
                             Node *node, Symbol *scope);
LLVMValueRef addrCastAs(CodeGenModule *codegen, LLVMBuilderRef builder,
                        Node *node, Symbol *scope);

LLVMValueRef genUnary(CodeGenModule *codegen, LLVMBuilderRef builder,
                      MIRValue *inst);
LLVMValueRef genBinary(CodeGenModule *codegen, LLVMBuilderRef builder,
                       MIRValue *inst);

// Function
void genFunctionBody(CodeGenModule *codegen, LLVMBuilderRef builder,
                     MIRValue *inst);
LLVMValueRef genCall(CodeGenModule *codegen, LLVMBuilderRef builder,
                     MIRValue *inst);

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
