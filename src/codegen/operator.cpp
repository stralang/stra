#include "../print.hpp"
#include "codegen.hpp"
#include "define.hpp"
#include "mir.hpp"
#include "llvm-c/Types.h"
#include <iostream>
#include <llvm-c/Core.h>

LLVMValueRef genUnary(CodeGenModule *codegen, LLVMBuilderRef builder,
                      MIRValue *inst) {
  Type *child_type = inst->unaryop.value->result_type;
  if (child_type->kind == TypeKind::SIMD) {
    child_type = child_type->slice.type;
  }

  LLVMValueRef value = getReference(codegen, inst->unaryop.value);

  switch (inst->unaryop.opcode) {
  case MIROpcode::Minus: {
    if (child_type->kind == TypeKind::Integer) {
      return LLVMBuildNeg(builder, value, "");
    } else if (child_type->kind == TypeKind::Float) {
      return LLVMBuildFNeg(builder, value, "");
    }

    break;
  }
  case MIROpcode::LogicalNot: {
    if (child_type->kind == TypeKind::Bool) {
      return LLVMBuildNot(builder, value, "");
    } else if (child_type->kind == TypeKind::Integer) {
      LLVMValueRef zero =
          LLVMConstInt(typeToLLVM(codegen, child_type), 0, false);
      return LLVMBuildICmp(builder, LLVMIntEQ, value, zero, "");
    } else if (child_type->kind == TypeKind::Float) {
      LLVMValueRef zero = LLVMConstReal(typeToLLVM(codegen, child_type), 0.0);
      return LLVMBuildFCmp(builder, LLVMRealOEQ, value, zero, "");
    }
    break;
  }
  case MIROpcode::BitwiseNot: {
    if (child_type->kind == TypeKind::Integer) {
      return LLVMBuildNot(builder, value, "");
    }
    break;
  }
  }

  return nullptr;
}

LLVMValueRef genCastAs(CodeGenModule *codegen, LLVMBuilderRef builder,
                       MIRValue *inst) {
  Type *src_type = inst->binop.lhs->result_type;
  Type *dst_type = inst->result_type;
  LLVMTypeRef dst_llvm_type = typeToLLVM(codegen, dst_type);

  // Reuse casts
  if (src_type->kind == TypeKind::Slice && dst_type->kind == TypeKind::Slice) {
    if (src_type->slice.length == 0 && dst_type->slice.length == 0) {
      return getReference(codegen, inst->binop.lhs);
    }

    if (src_type->slice.length > 0 && dst_type->slice.length == 0) {
      LLVMValueRef lhs_ptr = getReference(codegen, inst->binop.lhs);

      // Create Slice
      LLVMValueRef constants[2];
      constants[0] = LLVMConstNull(LLVMTypeOf(lhs_ptr));
      constants[1] = LLVMConstInt(
          LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size),
          src_type->slice.length, false);

      LLVMValueRef new_slice =
          LLVMConstStructInContext(codegen->ctx, constants, 2, false);
      new_slice = LLVMBuildInsertValue(builder, new_slice, lhs_ptr, 0, "");

      LLVMValueRef out_slice =
          BuildAlloca(codegen, builder, LLVMTypeOf(new_slice), "");
      LLVMBuildStore(builder, new_slice, out_slice);
      return out_slice;
    }
  }

  // Value casts
  if (src_type->kind == TypeKind::SIMD) {
    src_type = src_type->child;
  }

  LLVMValueRef lhs_value = getReference(codegen, inst->binop.lhs);
  if (src_type->kind == TypeKind::Bool || src_type->kind == TypeKind::Integer) {
    // Integer Cast
    if (dst_type->kind == TypeKind::Float && src_type->integer.is_signed) {
      return LLVMBuildSIToFP(builder, lhs_value, dst_llvm_type, "");
    } else if (dst_type->kind == TypeKind::Float &&
               !src_type->integer.is_signed) {
      return LLVMBuildUIToFP(builder, lhs_value, dst_llvm_type, "");
    } else if (dst_type->kind == TypeKind::Pointer) {
      return LLVMBuildIntToPtr(builder, lhs_value,
                               typeToLLVM(codegen, dst_type), "");
    }

    return LLVMBuildIntCast2(builder, lhs_value, dst_llvm_type,
                             src_type->integer.is_signed, "");
  } else if (src_type->kind == TypeKind::Float) {
    // Float Cast
    if (dst_type->kind == TypeKind::Integer && dst_type->integer.is_signed) {
      return LLVMBuildFPToSI(builder, lhs_value, dst_llvm_type, "");
    } else if (dst_type->kind == TypeKind::Integer &&
               !dst_type->integer.is_signed) {
      return LLVMBuildFPToUI(builder, lhs_value, dst_llvm_type, "");
    }

    return LLVMBuildFPCast(builder, lhs_value, dst_llvm_type, "");
  } else if (src_type->kind == TypeKind::Pointer) {
    // Pointer Cast
    if (dst_type->kind == TypeKind::Integer) {
      return LLVMBuildPtrToInt(
          builder, lhs_value,
          LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size), "");
    }

    return LLVMBuildPointerCast(builder, lhs_value, dst_llvm_type, "");
  } else if (src_type->kind == TypeKind::Enum) {
    return LLVMBuildIntCast2(builder, lhs_value, dst_llvm_type,
                             src_type->_enum.repr_type->integer.is_signed, "");
  }

  std::cerr << "Unhandled `as` cast in codegen\n";
  std::cerr << "Src `" << *src_type << "`\nDst `" << *dst_type << "`\n";
  std::abort();
}

LLVMValueRef genBinary(CodeGenModule *codegen, LLVMBuilderRef builder,
                       MIRValue *inst) {

  Type *lhs_type = inst->binop.lhs->result_type;
  Type *rhs_type = inst->binop.rhs->result_type;

  // Cast
  if (inst->binop.opcode == MIROpcode::As) {
    return genCastAs(codegen, builder, inst);
  } else if (inst->binop.opcode == MIROpcode::Bitcast) {
    LLVMValueRef lhs_value = getReference(codegen, inst->binop.lhs);
    LLVMTypeRef dest_ty = typeToLLVM(codegen, inst->binop.rhs->literal._typeid);
    return LLVMBuildBitCast(builder, lhs_value, dest_ty, "");
  }

  if (lhs_type->kind == TypeKind::SIMD) {
    lhs_type = lhs_type->child;
  }

  LLVMValueRef lhs_value = getReference(codegen, inst->binop.lhs);
  LLVMValueRef rhs_value = getReference(codegen, inst->binop.rhs);

  switch (inst->binop.opcode) {
  case MIROpcode::Add: {
    if (lhs_type->kind == TypeKind::Integer ||
        lhs_type->kind == TypeKind::Pointer) {
      return LLVMBuildAdd(builder, lhs_value, rhs_value, "");
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFAdd(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case MIROpcode::Sub: {
    if (lhs_type->kind == TypeKind::Integer ||
        lhs_type->kind == TypeKind::Pointer) {
      return LLVMBuildSub(builder, lhs_value, rhs_value, "");
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFSub(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case MIROpcode::Mul: {
    if (lhs_type->kind == TypeKind::Integer) {
      return LLVMBuildMul(builder, lhs_value, rhs_value, "");
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFMul(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case MIROpcode::Div: {
    if (lhs_type->kind == TypeKind::Integer) {
      if (lhs_type->integer.is_signed) {
        return LLVMBuildSDiv(builder, lhs_value, rhs_value, "");
      } else {
        return LLVMBuildUDiv(builder, lhs_value, rhs_value, "");
      }
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFDiv(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case MIROpcode::Mod: {
    if (lhs_type->kind == TypeKind::Integer) {
      if (lhs_type->integer.is_signed) {
        return LLVMBuildSRem(builder, lhs_value, rhs_value, "");
      } else {
        return LLVMBuildURem(builder, lhs_value, rhs_value, "");
      }
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFDiv(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case MIROpcode::Or: {
    return LLVMBuildOr(builder, lhs_value, rhs_value, "");
    break;
  }
  case MIROpcode::Xor: {
    return LLVMBuildXor(builder, lhs_value, rhs_value, "");

    break;
  }
  case MIROpcode::And: {
    return LLVMBuildAnd(builder, lhs_value, rhs_value, "");
    break;
  }
  case MIROpcode::LeftShift: {
    return LLVMBuildShl(builder, lhs_value, rhs_value, "");
    break;
  }
  case MIROpcode::RightShift: {
    return LLVMBuildLShr(builder, lhs_value, rhs_value, "");
    break;
  }
  case MIROpcode::EqualTo: {
    if (lhs_type->kind == TypeKind::Bool ||
        lhs_type->kind == TypeKind::Integer ||
        lhs_type->kind == TypeKind::Pointer) {
      return LLVMBuildICmp(builder, LLVMIntEQ, lhs_value, rhs_value, "");
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFCmp(builder, LLVMRealOEQ, lhs_value, rhs_value, "");
    }
    break;
  }
  case MIROpcode::NotEqualTo: {
    if (lhs_type->kind == TypeKind::Bool ||
        lhs_type->kind == TypeKind::Integer ||
        lhs_type->kind == TypeKind::Pointer) {
      return LLVMBuildICmp(builder, LLVMIntNE, lhs_value, rhs_value, "");
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFCmp(builder, LLVMRealONE, lhs_value, rhs_value, "");
    }
    break;
  }
  case MIROpcode::LessThen: {
    if (lhs_type->kind == TypeKind::Integer) {
      if (lhs_type->integer.is_signed) {
        return LLVMBuildICmp(builder, LLVMIntSLT, lhs_value, rhs_value, "");
      } else {
        return LLVMBuildICmp(builder, LLVMIntULT, lhs_value, rhs_value, "");
      }
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFCmp(builder, LLVMRealOLT, lhs_value, rhs_value, "");
    }
    break;
  }
  case MIROpcode::GreaterThen: {
    if (lhs_type->kind == TypeKind::Integer) {
      if (lhs_type->integer.is_signed) {
        return LLVMBuildICmp(builder, LLVMIntSGT, lhs_value, rhs_value, "");
      } else {
        return LLVMBuildICmp(builder, LLVMIntUGT, lhs_value, rhs_value, "");
      }
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFCmp(builder, LLVMRealOGT, lhs_value, rhs_value, "");
    }
    break;
  }
  case MIROpcode::LessThenOrEqualTo: {
    if (lhs_type->kind == TypeKind::Integer) {
      if (lhs_type->integer.is_signed) {
        return LLVMBuildICmp(builder, LLVMIntSLE, lhs_value, rhs_value, "");
      } else {
        return LLVMBuildICmp(builder, LLVMIntULE, lhs_value, rhs_value, "");
      }
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFCmp(builder, LLVMRealOLE, lhs_value, rhs_value, "");
    }
    break;
  }
  case MIROpcode::GreaterThenOrEqualTo: {
    if (lhs_type->kind == TypeKind::Integer) {
      if (lhs_type->integer.is_signed) {
        return LLVMBuildICmp(builder, LLVMIntSGE, lhs_value, rhs_value, "");
      } else {
        return LLVMBuildICmp(builder, LLVMIntUGE, lhs_value, rhs_value, "");
      }
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFCmp(builder, LLVMRealOGE, lhs_value, rhs_value, "");
    }
    break;
  }
  }

  return nullptr;
}
