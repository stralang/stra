#include "general.hpp"
#include <iostream>
#include <llvm-c/Core.h>

extern TargetABI ABIcreateSystemVAmd64();

TargetABI ABIcreateTarget(ABI abi) {
  switch (abi) {
  case ABI::SystemV_Amd64: {
    return ABIcreateSystemVAmd64();
  }
  }

  std::cerr << "Cannot create abi target for `" << (int)abi << "`\n";
  std::abort();
}

// Helpers

LLVMValueRef BuildABICast(LLVMBuilderRef builder, LLVMValueRef src,
                          LLVMTypeRef dest_ty) {
  LLVMTypeRef src_ty = LLVMTypeOf(src);
  LLVMTypeKind src_kind = LLVMGetTypeKind(src_ty);
  LLVMTypeKind dest_kind = LLVMGetTypeKind(dest_ty);

  if (src_kind == LLVMIntegerTypeKind && dest_kind == LLVMIntegerTypeKind) {
    return LLVMBuildIntCast2(builder, src, dest_ty, true, "");
  } else if (src_kind != LLVMStructTypeKind &&
             dest_kind != LLVMStructTypeKind) {
    return LLVMBuildBitCast(builder, src, dest_ty, "");
  }

  // Bitcast struct
  LLVMValueRef val = LLVMBuildAlloca(builder, src_ty, "");
  LLVMBuildStore(builder, src, val);
  val = LLVMBuildBitCast(builder, val, LLVMPointerType(dest_ty, 0), "");
  return LLVMBuildLoad2(builder, dest_ty, val, "");
}
