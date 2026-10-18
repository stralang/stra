#pragma once

#include "codegen.hpp"
#include "llvm-c/Types.h"
#include <llvm-c/Core.h>

// Base
LLVMValueRef getReference(CodeGenModule *codegen, RIRValueId value_id);
void gen(CodeGenModule *codegen, LLVMBuilderRef builder, RIRValueId inst);
void genDeclaration(CodeGenModule *codegen, RIRValueId inst);

// External reference
LLVMValueRef genExternal(CodeGenModule *codegen, LLVMBuilderRef builder,
                         RIRValue *inst);

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

// LLVMValueRef genCallBuiltin(CodeGenModule *codegen, LLVMBuilderRef builder,
//                             Node *builtin_name, Value *callee,
//                             Slice<LLVMValueRef> args);

// Assembly
LLVMValueRef genAssembly(CodeGenModule *codegen, LLVMBuilderRef builder,
                         RIRValue *inst);

// Helpers
void injectDefer(CodeGenModule *codegen, LLVMBuilderRef builder, Symbol *scope,
                 bool is_loop);

// A temporary trap handler
// NOTE: This will be replaced by a panic handler
inline void buildTrap(CodeGenModule *codegen, LLVMBuilderRef builder) {
  LLVMTypeRef params[1] = {LLVMInt32TypeInContext(codegen->ctx)};
  LLVMTypeRef func_ty =
      LLVMFunctionType(LLVMVoidTypeInContext(codegen->ctx), params, 1, false);

  LLVMValueRef trap_function = LLVMGetNamedFunction(codegen->mod, "exit");
  if (trap_function == nullptr) {
    trap_function = LLVMAddFunction(codegen->mod, "exit", func_ty);
  }

  LLVMValueRef args[1] = {
      LLVMConstInt(LLVMInt32TypeInContext(codegen->ctx), 1, false)};
  LLVMBuildCall2(builder, func_ty, trap_function, args, 1, "");
}

inline LLVMValueRef BuildAlloca(CodeGenModule *codegen, LLVMBuilderRef builder,
                                LLVMTypeRef ty, const char *name) {
  LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(builder);
  LLVMPositionBuilderAtEnd(builder, codegen->define_block);

  LLVMValueRef value = LLVMBuildAlloca(builder, ty, name);

  LLVMPositionBuilderAtEnd(builder, insert_block);
  return value;
}
