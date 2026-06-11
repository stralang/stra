#pragma once

#include "codegen.hpp"

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
                            Type *callee_type, Slice<LLVMValueRef> args);
