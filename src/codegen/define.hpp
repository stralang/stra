#pragma once

#include "codegen.hpp"
#include "llvm-c/Types.h"
#include <llvm-c/Core.h>

// Base
LLVMValueRef getReference(CodeGenModule *codegen, RIRValueId value_id);
void gen(CodeGenModule *codegen, LLVMBuilderRef builder, RIRValueId inst);
void genDeclaration(CodeGenModule *codegen, RIRValueId inst);

// Conversion
LLVMTypeRef typeToLLVM(CodeGenModule *codegen, RIRTypeId type_id,
                       const char *name = nullptr);
LLVMValueRef constantToLLVM(CodeGenModule *codegen, RIRConstant *constant,
                            RIRTypeId const_type);

// Operator
LLVMValueRef genUnary(CodeGenModule *codegen, LLVMBuilderRef builder,
                      RIRValue *inst);
LLVMValueRef genBinary(CodeGenModule *codegen, LLVMBuilderRef builder,
                       RIRValue *inst);
LLVMValueRef genCast(CodeGenModule *codegen, LLVMBuilderRef builder,
                     RIRValue *inst);

// Function
void genFunctionBody(CodeGenModule *codegen, LLVMBuilderRef builder,
                     RIRValue *inst);
LLVMValueRef genCall(CodeGenModule *codegen, LLVMBuilderRef builder,
                     RIRValue *inst);

LLVMValueRef genCallBuiltin(CodeGenModule *codegen, LLVMBuilderRef builder,
                            Node *builtin_name, Value *callee,
                            Slice<LLVMValueRef> args);

// Assembly
LLVMValueRef genAssembly(CodeGenModule *codegen, LLVMBuilderRef builder,
                         RIRValue *inst);

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
