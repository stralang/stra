#include "../containers.hpp"
#include "general.hpp"
#include "llvm-c/Types.h"
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/Types.h>

/// NOTE: The abi classification is referenced and copied from:
///       Odin, Clang, and the SystemV x86_64 documentation

enum class RegClass : uint8_t {
  Integer,
  SSE,
  SSEUP,
  X87,
  X87UP,
  ComplexX87,
  NO_CLASS,
  MEMORY,
};

void unify(Slice<RegClass> *out, size_t i, RegClass new_class) {
  RegClass old_class = out->ptr[i];

  if (old_class == new_class) {
    return;
  }

  RegClass set = RegClass::SSE;
  if (old_class == RegClass::NO_CLASS) {
    set = new_class;
  } else if (new_class == RegClass::NO_CLASS) {
    set = old_class;
  } else if (old_class == RegClass::MEMORY || new_class == RegClass::MEMORY) {
    set = RegClass::MEMORY;
  } else if (old_class == RegClass::Integer || new_class == RegClass::Integer) {
    set = RegClass::Integer;
  } else if (old_class == RegClass::X87 || old_class == RegClass::X87UP ||
             old_class == RegClass::ComplexX87 || new_class == RegClass::X87 ||
             new_class == RegClass::X87UP ||
             new_class == RegClass::ComplexX87) {
    set = RegClass::MEMORY;
  }

  out->ptr[i] = set;
}

void classify(Slice<RegClass> *out, LLVMTargetDataRef target_data,
              LLVMTypeRef ty, size_t off) {
  LLVMTypeKind kind = LLVMGetTypeKind(ty);
  size_t byte_size = (LLVMSizeOfTypeInBits(target_data, ty) + 7) / 8;

  switch (kind) {
  case LLVMIntegerTypeKind: {
    size_t s = byte_size + 7;
    while (s >= 8) {
      unify(out, off / 8, RegClass::Integer);
      off += 8;
      s -= 8;
    }
    break;
  }
  case LLVMPointerTypeKind: {
    unify(out, off / 8, RegClass::Integer);
    break;
  }
  case LLVMHalfTypeKind:
  case LLVMFloatTypeKind:
  case LLVMDoubleTypeKind: {
    unify(out, off / 8, RegClass::Integer);
    break;
  }
  case LLVMX86_FP80TypeKind: {
    unify(out, off / 8, RegClass::X87);
    break;
  }
  case LLVMFP128TypeKind: {
    unify(out, off / 8, RegClass::SSE);
    unify(out, off / 8 + 1, RegClass::SSEUP);
    break;
  }
  case LLVMArrayTypeKind: {
    size_t len = LLVMGetArrayLength(ty);
    LLVMTypeRef elem_ty = LLVMGetElementType(ty);
    size_t elem_size = (LLVMSizeOfTypeInBits(target_data, elem_ty) + 7) / 8;

    for (size_t i = 0; i < len; i++) {
      classify(out, target_data, elem_ty, off);
      off += elem_size;
    }
    break;
  }
  case LLVMVectorTypeKind: {
    size_t len = LLVMGetVectorSize(ty);
    LLVMTypeRef elem_ty = LLVMGetElementType(ty);
    size_t elem_size = (LLVMSizeOfTypeInBits(target_data, elem_ty) + 7) / 8;

    RegClass _class = RegClass::SSE;
    for (size_t i = 0; i < len; i++) {
      unify(out, off / 8, _class);
      _class = RegClass::SSEUP;
      off += elem_size;
    }
    break;
  }
  case LLVMStructTypeKind: {
    bool is_packed = LLVMIsPackedStruct(ty);
    size_t field_count = LLVMCountStructElementTypes(ty);
    for (size_t i = 0; i < field_count; i++) {
      LLVMTypeRef elem_ty = LLVMStructGetTypeAtIndex(ty, i);
      size_t elem_size = (LLVMSizeOfTypeInBits(target_data, elem_ty) + 7) / 8;
      if (!is_packed) {
        size_t align = LLVMABIAlignmentOfType(target_data, elem_ty);
        off = (off + align - 1) / align * align;
      }

      classify(out, target_data, elem_ty, off);
      off += elem_size;
    }
    break;
  }
  }
}

ABIArg classifyReturnType(LLVMModuleRef mod, LLVMTypeRef ty) {
  LLVMContextRef ctx = LLVMGetModuleContext(mod);
  LLVMTargetDataRef target_data = LLVMGetModuleDataLayout(mod);

  // Classify
  size_t class_count = (LLVMSizeOfTypeInBits(target_data, ty) + 63) / 64;
  Slice<RegClass> classes = {
      .len = class_count,
      .ptr = (RegClass *)malloc(sizeof(RegClass) * class_count),
  };
  memset(classes.ptr, (uint8_t)RegClass::NO_CLASS, classes.len);
  classify(&classes, target_data, ty, 0);

  free(classes.ptr);
  return {.kind = ABIArgKind::Direct, .type = ty};
}

ABIArg classifyArgumentType(LLVMModuleRef mod, LLVMTypeRef ty) {
  LLVMContextRef ctx = LLVMGetModuleContext(mod);
  LLVMTargetDataRef target_data = LLVMGetModuleDataLayout(mod);

  // Classify
  size_t class_count = (LLVMSizeOfTypeInBits(target_data, ty) + 63) / 64;
  Slice<RegClass> classes = {
      .len = class_count,
      .ptr = (RegClass *)malloc(sizeof(RegClass) * class_count),
  };
  memset(classes.ptr, (uint8_t)RegClass::NO_CLASS, classes.len);
  classify(&classes, target_data, ty, 0);

  free(classes.ptr);
  return {.kind = ABIArgKind::Direct, .type = ty};
}

extern TargetABI ABIcreateSystemVAmd64() {
  return {.classifyReturnType = classifyReturnType,
          .classifyArgumentType = classifyArgumentType};
}
