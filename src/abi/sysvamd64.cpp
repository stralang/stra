#include "../containers.hpp"
#include "general.hpp"
#include "llvm-c/Types.h"
#include <cstddef>
#include <cstdint>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/Types.h>

/// NOTE: The abi handling is copied from Odin

enum class RegClass : uint8_t {
  Integer,
  SSEHalfScalar,
  SSEFloatScalar,
  SSEHalf,
  SSEFloat,
  SSEDouble,
  SSEInt8,
  SSEInt16,
  SSEInt32,
  SSEInt64,
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

  RegClass set = new_class;
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
  } else if (new_class == RegClass::SSEUP) {
    switch (old_class) {
    case RegClass::SSEHalfScalar:
    case RegClass::SSEFloatScalar:
    case RegClass::SSEHalf:
    case RegClass::SSEFloat:
    case RegClass::SSEDouble:
    case RegClass::SSEInt8:
    case RegClass::SSEInt16:
    case RegClass::SSEInt32:
    case RegClass::SSEInt64: {
      return;
    }
    }
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
  case LLVMHalfTypeKind: {
    RegClass _class =
        (off % 8 != 0) ? RegClass::SSEHalf : RegClass::SSEHalfScalar;
    unify(out, off / 8, _class);
    break;
  }
  case LLVMFloatTypeKind: {
    RegClass _class =
        (off % 8 == 4) ? RegClass::SSEFloat : RegClass::SSEFloatScalar;
    unify(out, off / 8, _class);
    break;
  }
  case LLVMDoubleTypeKind: {
    unify(out, off / 8, RegClass::SSEDouble);
    break;
  }
  case LLVMX86_FP80TypeKind: {
    unify(out, off / 8, RegClass::X87);
    break;
  }
  case LLVMFP128TypeKind: {
    unify(out, off / 8, RegClass::SSEDouble);
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

    // Get element specfic class
    RegClass _class;
    LLVMTypeKind elem_kind = LLVMGetTypeKind(elem_ty);
    switch (elem_kind) {
    case LLVMIntegerTypeKind: {
      size_t int_width = LLVMGetIntTypeWidth(elem_ty);
      if (int_width <= 8) {
        _class = RegClass::SSEInt8;
      } else if (int_width <= 16) {
        _class = RegClass::SSEInt16;
      } else if (int_width <= 32) {
        _class = RegClass::SSEInt32;
      } else if (int_width <= 64) {
        _class = RegClass::SSEInt64;
      } else {
        for (size_t i = 0; i < len; i++) {
          classify(out, target_data, elem_ty, off + i * elem_size);
        }
      }
      break;
    }
    case LLVMHalfTypeKind: {
      _class = RegClass::SSEHalf;
      break;
    }
    case LLVMFloatTypeKind: {
      _class = RegClass::SSEFloat;
      break;
    }
    case LLVMDoubleTypeKind: {
      _class = RegClass::SSEDouble;
      break;
    }
    }

    // Add Vector classes
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

size_t count_sse(Slice<RegClass> classes, size_t i) {
  size_t len = 0;
  while (i < classes.len) {
    if (classes.ptr[i] != RegClass::SSEUP) {
      break;
    }

    i += 1;
    len += 1;
  }

  return len;
}

bool is_register(LLVMTargetDataRef target_data, LLVMTypeRef ty) {
  LLVMTypeKind kind = LLVMGetTypeKind(ty);
  size_t size = (LLVMSizeOfTypeInBits(target_data, ty) + 7) / 8;
  if (size == 0) {
    return false;
  }

  switch (kind) {
  case LLVMIntegerTypeKind: {
    return LLVM_VERSION_MAJOR >= 18 && size >= 16;
  }
  case LLVMHalfTypeKind:
  case LLVMFloatTypeKind:
  case LLVMDoubleTypeKind:
  case LLVMPointerTypeKind: {
    return true;
  }
  }
  return false;
}

LLVMTypeRef llreg(LLVMContextRef ctx, LLVMTargetDataRef target_data,
                  Slice<RegClass> classes, LLVMTypeRef ty) {
  Allocator allocator;
  ArrayList<LLVMTypeRef> types;
  types.init(&allocator, classes.len);
  size_t size = (LLVMSizeOfTypeInBits(target_data, ty) + 7) / 8;

  size_t i = 0;
  while (i < classes.len) {
    RegClass cls = classes.ptr[i];
    switch (cls) {
    case RegClass::Integer: {
      size_t reg_size = size >= 8 ? 8 : size;
      types.push(LLVMIntTypeInContext(ctx, reg_size * 8));
      size -= reg_size;
      break;
    }
    case RegClass::SSEHalf:
    case RegClass::SSEFloat:
    case RegClass::SSEDouble:
    case RegClass::SSEInt8:
    case RegClass::SSEInt16:
    case RegClass::SSEInt32:
    case RegClass::SSEInt64: {
      size_t vec_len = count_sse(classes, i + 1) + 1;
      size_t words_per_elem = 0;
      LLVMTypeRef elem_ty;

      if (cls == RegClass::SSEHalf) {
        words_per_elem = 4;
        elem_ty = LLVMHalfTypeInContext(ctx);
      } else if (cls == RegClass::SSEFloat) {
        words_per_elem = 2;
        elem_ty = LLVMFloatTypeInContext(ctx);
      } else if (cls == RegClass::SSEDouble) {
        words_per_elem = 1;
        elem_ty = LLVMDoubleTypeInContext(ctx);
      } else if (cls == RegClass::SSEInt8) {
        words_per_elem = 8;
        elem_ty = LLVMInt8TypeInContext(ctx);
      } else if (cls == RegClass::SSEInt16) {
        words_per_elem = 4;
        elem_ty = LLVMInt16TypeInContext(ctx);
      } else if (cls == RegClass::SSEInt32) {
        words_per_elem = 2;
        elem_ty = LLVMInt32TypeInContext(ctx);
      } else if (cls == RegClass::SSEInt64) {
        words_per_elem = 1;
        elem_ty = LLVMInt64TypeInContext(ctx);
      }

      if (vec_len * words_per_elem == 1) {
        types.push(elem_ty);
      } else {
        LLVMTypeRef vector_ty =
            LLVMVectorType(elem_ty, vec_len * words_per_elem);
        types.push(vector_ty);
      }

      size -= vec_len * words_per_elem;
      i += vec_len;
      break;
    }
    case RegClass::SSEHalfScalar: {
      types.push(LLVMHalfTypeInContext(ctx));
      break;
    }
    case RegClass::SSEFloatScalar: {
      types.push(LLVMFloatTypeInContext(ctx));
      break;
    }
    }

    i += 1;
  }

  if (types.length == 1) {
    return types.data.ptr[0];
  }

  return LLVMStructTypeInContext(ctx, types.data.ptr, classes.len, false);
}

ABIArg argize(LLVMContextRef ctx, LLVMTargetDataRef target_data, LLVMTypeRef ty,
              bool is_arg, int32_t *int_regs, int32_t *sse_regs) {
  if (LLVMGetTypeKind(ty) == LLVMVoidTypeKind) {
    return {.kind = ABIArgKind::Ignore};
  }

  // Classify
  size_t class_count = (LLVMSizeOfTypeInBits(target_data, ty) + 63) / 64;
  Slice<RegClass> classes = {
      .len = class_count,
      .ptr = (RegClass *)malloc(sizeof(RegClass) * class_count),
  };
  memset(classes.ptr, (uint8_t)RegClass::NO_CLASS, classes.len);
  classify(&classes, target_data, ty, 0);

  // Get register count
  int32_t needed_int = 0;
  int32_t needed_sse = 0;
  bool is_memory = false;
  for (size_t i = 0; i < classes.len; i++) {
    switch (classes[i]) {
    case RegClass::Integer: {
      needed_int += 1;
      break;
    }
    case RegClass::SSEHalfScalar:
    case RegClass::SSEFloatScalar:
    case RegClass::SSEHalf:
    case RegClass::SSEFloat:
    case RegClass::SSEDouble:
    case RegClass::SSEInt8:
    case RegClass::SSEInt16:
    case RegClass::SSEInt32:
    case RegClass::SSEInt64:
    case RegClass::SSEUP: {
      needed_sse += 1;
      break;
    }
    case RegClass::X87:
    case RegClass::MEMORY: {
      is_memory = true;
      break;
    }
    }
  }

  bool ran_out_of_regs = false;
  if (int_regs != nullptr && sse_regs != nullptr) {
    *int_regs -= needed_int;
    *sse_regs -= needed_sse;
    bool int_ok = *int_regs >= 0;
    bool sse_ok = *sse_regs >= 0;

    *int_regs = int_ok ? *int_regs : 0;
    *sse_regs = sse_ok ? *sse_regs : 0;

    LLVMTypeKind kind = LLVMGetTypeKind(ty);
    ran_out_of_regs = kind == LLVMStructTypeKind || kind == LLVMArrayTypeKind;
  }

  if (is_register(target_data, ty)) {
    free(classes.ptr);
    return {.kind = ABIArgKind::Direct, .type = ty};
  } else if (ran_out_of_regs) {
    free(classes.ptr);
    if (is_arg) {
      return {.kind = ABIArgKind::Indirect, .type = ty, .byval = true};
    }

    size_t attr_kind = LLVMGetEnumAttributeKindForName("sret", 4);
    return {
        .kind = ABIArgKind::Indirect,
        .type = ty,
        .attribute =
            LLVMCreateTypeAttribute(ctx, attr_kind, LLVMPointerType(ty, 0)),
    };
  } else if (is_memory) {
    free(classes.ptr);
    if (is_arg) {
      size_t attr_kind = LLVMGetEnumAttributeKindForName("sret", 4);
      return {
          .kind = ABIArgKind::Indirect,
          .type = ty,
          .attribute =
              LLVMCreateTypeAttribute(ctx, attr_kind, LLVMPointerType(ty, 0)),
      };
    }

    return {.kind = ABIArgKind::Indirect, .type = ty, .byval = true};
  }

  return {.kind = ABIArgKind::Direct,
          .type = llreg(ctx, target_data, classes, ty)};
}

ABIArg classifyReturnType(LLVMModuleRef mod, LLVMTypeRef ty) {
  LLVMContextRef ctx = LLVMGetModuleContext(mod);
  LLVMTargetDataRef target_data = LLVMGetModuleDataLayout(mod);

  ABIArg arg = argize(ctx, target_data, ty, false, nullptr, nullptr);
  return arg;
}

ABIArg classifyArgumentType(LLVMModuleRef mod, LLVMTypeRef ty) {
  LLVMContextRef ctx = LLVMGetModuleContext(mod);
  LLVMTargetDataRef target_data = LLVMGetModuleDataLayout(mod);

  ABIArg arg = argize(ctx, target_data, ty, true, nullptr, nullptr);
  return arg;
}

extern TargetABI ABIcreateSystemVAmd64() {
  return {.classifyReturnType = classifyReturnType,
          .classifyArgumentType = classifyArgumentType};
}
