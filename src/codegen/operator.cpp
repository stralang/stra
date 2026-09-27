#include "../print.hpp"
#include "codegen.hpp"
#include "define.hpp"
#include "uir/uir.hpp"
#include "llvm-c/Types.h"
#include <iostream>
#include <llvm-c/Core.h>

LLVMValueRef genUnary(CodeGenModule *codegen, LLVMBuilderRef builder,
                      RIRValue *inst) {
  RIRValueId operand_id = inst->unaryop.value;
  RIRValue *operand = codegen->rir_context->getInst(operand_id);
  RIRType *operand_type = codegen->rir_context->getType(operand->result);
  LLVMValueRef value = getReference(codegen, operand_id);

  switch (inst->unaryop.opcode) {
  case RIROpcode::Minus: {
    if (operand_type->kind == RIRTypeKind::Integer) {
      return LLVMBuildNeg(builder, value, "");
    } else if (operand_type->kind == RIRTypeKind::Float) {
      return LLVMBuildFNeg(builder, value, "");
    }

    break;
  }
  case RIROpcode::LogicalNot: {
    if (operand_type->kind == RIRTypeKind::Bool) {
      return LLVMBuildNot(builder, value, "");
    } else if (operand_type->kind == RIRTypeKind::Integer) {
      LLVMValueRef zero =
          LLVMConstInt(typeToLLVM(codegen, operand->result), 0, false);
      return LLVMBuildICmp(builder, LLVMIntEQ, value, zero, "");
    } else if (operand_type->kind == RIRTypeKind::Float) {
      LLVMValueRef zero =
          LLVMConstReal(typeToLLVM(codegen, operand->result), 0.0);
      return LLVMBuildFCmp(builder, LLVMRealOEQ, value, zero, "");
    }
    break;
  }
  case RIROpcode::BitwiseNot: {
    if (operand_type->kind == RIRTypeKind::Integer) {
      return LLVMBuildNot(builder, value, "");
    }
    break;
  }
  }

  return nullptr;
}

LLVMValueRef genBinary(CodeGenModule *codegen, LLVMBuilderRef builder,
                       RIRValue *inst) {

  LLVMValueRef lhs_value = getReference(codegen, inst->binop.lhs);
  LLVMValueRef rhs_value = getReference(codegen, inst->binop.rhs);

  RIRValue *lhs_inst = codegen->rir_context->getInst(inst->binop.lhs);
  RIRType *lhs_type = codegen->rir_context->getType(lhs_inst->result);

  switch (inst->binop.opcode) {
  case RIROpcode::Add: {
    if (lhs_type->kind == RIRTypeKind::Integer ||
        lhs_type->kind == RIRTypeKind::Pointer) {
      return LLVMBuildAdd(builder, lhs_value, rhs_value, "");
    } else if (lhs_type->kind == RIRTypeKind::Float) {
      return LLVMBuildFAdd(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case RIROpcode::Sub: {
    if (lhs_type->kind == RIRTypeKind::Integer ||
        lhs_type->kind == RIRTypeKind::Pointer) {
      return LLVMBuildSub(builder, lhs_value, rhs_value, "");
    } else if (lhs_type->kind == RIRTypeKind::Float) {
      return LLVMBuildFSub(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case RIROpcode::Mul: {
    if (lhs_type->kind == RIRTypeKind::Integer) {
      return LLVMBuildMul(builder, lhs_value, rhs_value, "");
    } else if (lhs_type->kind == RIRTypeKind::Float) {
      return LLVMBuildFMul(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case RIROpcode::Div: {
    if (lhs_type->kind == RIRTypeKind::Integer) {
      if (lhs_type->integer.is_signed) {
        return LLVMBuildSDiv(builder, lhs_value, rhs_value, "");
      } else {
        return LLVMBuildUDiv(builder, lhs_value, rhs_value, "");
      }
    } else if (lhs_type->kind == RIRTypeKind::Float) {
      return LLVMBuildFDiv(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case RIROpcode::Mod: {
    if (lhs_type->kind == RIRTypeKind::Integer) {
      if (lhs_type->integer.is_signed) {
        return LLVMBuildSRem(builder, lhs_value, rhs_value, "");
      } else {
        return LLVMBuildURem(builder, lhs_value, rhs_value, "");
      }
    } else if (lhs_type->kind == RIRTypeKind::Float) {
      return LLVMBuildFDiv(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case RIROpcode::Or: {
    return LLVMBuildOr(builder, lhs_value, rhs_value, "");
    break;
  }
  case RIROpcode::Xor: {
    return LLVMBuildXor(builder, lhs_value, rhs_value, "");

    break;
  }
  case RIROpcode::And: {
    return LLVMBuildAnd(builder, lhs_value, rhs_value, "");
    break;
  }
  case RIROpcode::LeftShift: {
    return LLVMBuildShl(builder, lhs_value, rhs_value, "");
    break;
  }
  case RIROpcode::RightShift: {
    return LLVMBuildLShr(builder, lhs_value, rhs_value, "");
    break;
  }
  case RIROpcode::EqualTo: {
    if (lhs_type->kind == RIRTypeKind::Bool ||
        lhs_type->kind == RIRTypeKind::Integer ||
        lhs_type->kind == RIRTypeKind::Pointer) {
      return LLVMBuildICmp(builder, LLVMIntEQ, lhs_value, rhs_value, "");
    } else if (lhs_type->kind == RIRTypeKind::Float) {
      return LLVMBuildFCmp(builder, LLVMRealOEQ, lhs_value, rhs_value, "");
    }
    break;
  }
  case RIROpcode::NotEqualTo: {
    if (lhs_type->kind == RIRTypeKind::Bool ||
        lhs_type->kind == RIRTypeKind::Integer ||
        lhs_type->kind == RIRTypeKind::Pointer) {
      return LLVMBuildICmp(builder, LLVMIntNE, lhs_value, rhs_value, "");
    } else if (lhs_type->kind == RIRTypeKind::Float) {
      return LLVMBuildFCmp(builder, LLVMRealONE, lhs_value, rhs_value, "");
    }
    break;
  }
  case RIROpcode::LessThen: {
    if (lhs_type->kind == RIRTypeKind::Integer) {
      if (lhs_type->integer.is_signed) {
        return LLVMBuildICmp(builder, LLVMIntSLT, lhs_value, rhs_value, "");
      } else {
        return LLVMBuildICmp(builder, LLVMIntULT, lhs_value, rhs_value, "");
      }
    } else if (lhs_type->kind == RIRTypeKind::Float) {
      return LLVMBuildFCmp(builder, LLVMRealOLT, lhs_value, rhs_value, "");
    }
    break;
  }
  case RIROpcode::GreaterThen: {
    if (lhs_type->kind == RIRTypeKind::Integer) {
      if (lhs_type->integer.is_signed) {
        return LLVMBuildICmp(builder, LLVMIntSGT, lhs_value, rhs_value, "");
      } else {
        return LLVMBuildICmp(builder, LLVMIntUGT, lhs_value, rhs_value, "");
      }
    } else if (lhs_type->kind == RIRTypeKind::Float) {
      return LLVMBuildFCmp(builder, LLVMRealOGT, lhs_value, rhs_value, "");
    }
    break;
  }
  case RIROpcode::LessThenOrEqualTo: {
    if (lhs_type->kind == RIRTypeKind::Integer) {
      if (lhs_type->integer.is_signed) {
        return LLVMBuildICmp(builder, LLVMIntSLE, lhs_value, rhs_value, "");
      } else {
        return LLVMBuildICmp(builder, LLVMIntULE, lhs_value, rhs_value, "");
      }
    } else if (lhs_type->kind == RIRTypeKind::Float) {
      return LLVMBuildFCmp(builder, LLVMRealOLE, lhs_value, rhs_value, "");
    }
    break;
  }
  case RIROpcode::GreaterThenOrEqualTo: {
    if (lhs_type->kind == RIRTypeKind::Integer) {
      if (lhs_type->integer.is_signed) {
        return LLVMBuildICmp(builder, LLVMIntSGE, lhs_value, rhs_value, "");
      } else {
        return LLVMBuildICmp(builder, LLVMIntUGE, lhs_value, rhs_value, "");
      }
    } else if (lhs_type->kind == RIRTypeKind::Float) {
      return LLVMBuildFCmp(builder, LLVMRealOGE, lhs_value, rhs_value, "");
    }
    break;
  }
  }

  return nullptr;
}

LLVMValueRef genCast(CodeGenModule *codegen, LLVMBuilderRef builder,
                     RIRValue *inst) {
  // Bitcast
  if (inst->cast.bitcast) {
    LLVMValueRef lhs_value = getReference(codegen, inst->cast.value);
    LLVMTypeRef dest_ty = typeToLLVM(codegen, inst->result);
    return LLVMBuildBitCast(builder, lhs_value, dest_ty, "");
  }

  // Cast
  LLVMTypeRef dst_llvm_type = typeToLLVM(codegen, inst->result);

  RIRValue *src_inst = codegen->rir_context->getInst(inst->cast.value);
  RIRType *src_type = codegen->rir_context->getType(src_inst->result);
  RIRType *dst_type = codegen->rir_context->getType(inst->result);

  // Reuse casts
  if (src_type->kind == RIRTypeKind::Slice &&
      dst_type->kind == RIRTypeKind::Slice) {
    if (src_type->slice.length == 0 && dst_type->slice.length == 0) {
      return getReference(codegen, inst->cast.value);
    }

    if (src_type->slice.length > 0 && dst_type->slice.length == 0) {
      LLVMValueRef lhs_ptr = getReference(codegen, inst->cast.value);

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
  LLVMValueRef lhs_value = getReference(codegen, inst->cast.value);
  if (src_type->kind == RIRTypeKind::Bool ||
      src_type->kind == RIRTypeKind::Integer) {
    // Integer Cast
    if (dst_type->kind == RIRTypeKind::Float && src_type->integer.is_signed) {
      return LLVMBuildSIToFP(builder, lhs_value, dst_llvm_type, "");
    } else if (dst_type->kind == RIRTypeKind::Float &&
               !src_type->integer.is_signed) {
      return LLVMBuildUIToFP(builder, lhs_value, dst_llvm_type, "");
    } else if (dst_type->kind == RIRTypeKind::Pointer) {
      return LLVMBuildIntToPtr(builder, lhs_value, dst_llvm_type, "");
    }

    return LLVMBuildIntCast2(builder, lhs_value, dst_llvm_type,
                             src_type->integer.is_signed, "");
  } else if (src_type->kind == RIRTypeKind::Float) {
    // Float Cast
    if (dst_type->kind == RIRTypeKind::Integer && dst_type->integer.is_signed) {
      return LLVMBuildFPToSI(builder, lhs_value, dst_llvm_type, "");
    } else if (dst_type->kind == RIRTypeKind::Integer &&
               !dst_type->integer.is_signed) {
      return LLVMBuildFPToUI(builder, lhs_value, dst_llvm_type, "");
    }

    return LLVMBuildFPCast(builder, lhs_value, dst_llvm_type, "");
  } else if (src_type->kind == RIRTypeKind::Pointer) {
    // Pointer Cast
    if (dst_type->kind == RIRTypeKind::Integer) {
      return LLVMBuildPtrToInt(
          builder, lhs_value,
          LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size), "");
    }

    return LLVMBuildPointerCast(builder, lhs_value, dst_llvm_type, "");
  } else if (src_type->kind == RIRTypeKind::Enum) {
    RIRType *repr_type = codegen->rir_context->getType(src_type->_enum.repr);
    return LLVMBuildIntCast2(builder, lhs_value, dst_llvm_type,
                             repr_type->integer.is_signed, "");
  }

  std::cerr << "Unhandled `as` cast in codegen\n";
  std::cerr << "Src `" << src_type << "`\nDst `" << dst_type << "`\n";
  std::abort();
}
