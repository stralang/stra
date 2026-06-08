#include "general.hpp"

enum class RegClass {
  Integer,
  SSE,
  SSEUP,
  X87,
  X87UP,
  ComplexX87,
  NO_CLASS,
  MEMORY,
};

ABIArg classifyReturnType(LLVMContextRef ctx, LLVMTypeRef ty) {
  return {.kind = ABIArgKind::Direct, .type = ty};
}

ABIArg classifyArgumentType(LLVMContextRef ctx, LLVMTypeRef ty) {
  return {.kind = ABIArgKind::Direct, .type = ty};
}

extern TargetABI ABIcreateSystemVAmd64() {
  return {.classifyReturnType = classifyReturnType,
          .classifyArgumentType = classifyArgumentType};
}
