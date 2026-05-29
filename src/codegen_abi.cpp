#include "codegen.hpp"
#include "llvm-c/Types.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

extern LLVMTypeRef typeToLLVM(CodeGenModule *codegen, Type *type);

extern LLVMValueRef gen(CodeGenModule *codegen, LLVMBuilderRef builder,
                        Node *node, Symbol *scope);
extern LLVMValueRef addr(CodeGenModule *codegen, LLVMBuilderRef builder,
                         Node *node, Symbol *scope);

LLVMTypeRef argumentTypeToLLVM(CodeGenModule *codegen, Type *type) {
  if (type->kind == TypeKind::Struct) {
    size_t size = type->sizeBits(codegen->pointer_size);

    if (size <= 128) {
      if (size <= 64) {
        return LLVMIntTypeInContext(codegen->ctx, size);
      } else {
        LLVMTypeRef int_ty = LLVMIntTypeInContext(codegen->ctx, 64);
        return LLVMArrayType2(int_ty, 2);
      }
    }
  }

  return typeToLLVM(codegen, type);
}

LLVMValueRef argumentValue(CodeGenModule *codegen, LLVMBuilderRef builder,
                           Node *node, Symbol *scope) {
  if (node->value.type->kind == TypeKind::Struct) {
    size_t size = node->value.type->sizeBits(codegen->pointer_size);

    if (size <= 128) {
      LLVMValueRef _addr = addr(codegen, builder, node, scope);
      LLVMTypeRef load_type;
      if (size <= 64) {
        load_type = LLVMIntTypeInContext(codegen->ctx, size);
      } else {
        LLVMTypeRef int_ty = LLVMIntTypeInContext(codegen->ctx, 64);
        load_type = LLVMArrayType2(int_ty, 2);
      }

      return LLVMBuildLoad2(builder, load_type, _addr, "");
    }
  }

  return gen(codegen, builder, node, scope);
}

LLVMValueRef returnABIValue(CodeGenModule *codegen, LLVMBuilderRef builder,
                            Type *return_type, LLVMValueRef value) {
  if (return_type->kind == TypeKind::Struct) {
    size_t return_size = return_type->sizeBits(codegen->pointer_size);

    if (return_size <= 128) {
      LLVMTypeRef alloc_ty = LLVMIntTypeInContext(codegen->ctx, return_size);
      LLVMValueRef alloca = LLVMBuildAlloca(builder, alloc_ty, "");
      LLVMBuildStore(builder, value, alloca);

      LLVMTypeRef ty = typeToLLVM(codegen, return_type);
      return LLVMBuildLoad2(builder, ty, alloca, "");
    }
  }

  return value;
}
