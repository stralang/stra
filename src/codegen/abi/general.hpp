#pragma once

#include "uir/environment.hpp"
#include "llvm-c/Types.h"
#include <iostream>
#include <llvm-c/Types.h>

enum class ABIArgKind {
  Direct,
  Indirect,
  Ignore,
};

struct ABIArg {
  ABIArgKind kind = ABIArgKind::Ignore;
  LLVMTypeRef type = nullptr;
  LLVMAttributeRef attribute = nullptr;
  bool byval = false;
};

struct TargetABI {
  ABIArg (*classifyReturnType)(LLVMModuleRef mod, LLVMTypeRef ty);
  ABIArg (*classifyArgumentType)(LLVMModuleRef mod, LLVMTypeRef ty);
};

enum class ABI {
  SystemV_Amd64,
};

inline ABI GetABIFromTargetTriple(TargetTriple tt) {
  switch (tt.arch) {
  case Arch::x86_64: {
    if (tt.os == Os::Linux) {
      return ABI::SystemV_Amd64;
    }
    break;
  }
  }

  std::cerr << "No known ABI for Target Triple\n";
  std::abort();
}

TargetABI ABIcreateTarget(ABI abi);

// Helpers
LLVMValueRef BuildABICast(LLVMBuilderRef builder, LLVMValueRef value,
                          LLVMTypeRef dest_ty);
