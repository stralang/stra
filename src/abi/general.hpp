#pragma once

#include "llvm-c/Types.h"
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

TargetABI ABIcreateTarget(ABI abi);
