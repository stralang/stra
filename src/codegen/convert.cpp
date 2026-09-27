#include "codegen.hpp"
#include "define.hpp"
#include "rir/constant.hpp"
#include "rir/rir.hpp"
#include "rir/type.hpp"
#include "uir/literal.hpp"
#include "llvm-c/Types.h"
#include <cassert>
#include <llvm-c/Core.h>

LLVMTypeRef typeToLLVM(CodeGenModule *codegen, RIRTypeId type_id,
                       const char *name) {
  LLVMTypeRef *type_cache = codegen->type_to_llvm.get(type_id);
  if (type_cache != nullptr) {
    return *type_cache;
  }

  LLVMTypeRef out;
  RIRType *type = codegen->rir_context->getType(type_id);
  switch (type->kind) {
  case RIRTypeKind::Void: {
    out = LLVMVoidTypeInContext(codegen->ctx);
    break;
  }
  case RIRTypeKind::Bool: {
    out = LLVMInt1TypeInContext(codegen->ctx);
    break;
  }
  case RIRTypeKind::Integer: {
    size_t bits = type->integer.bits;
    if (type->integer.bits == -1) {
      bits = codegen->pointer_size;
    }
    out = LLVMIntTypeInContext(codegen->ctx, bits);
    break;
  }
  case RIRTypeKind::Float: {
    switch (type->float_bits) {
    case 16: {
      out = LLVMHalfTypeInContext(codegen->ctx);
      break;
    }
    case 32: {
      out = LLVMFloatTypeInContext(codegen->ctx);
      break;
    }
    case 64: {
      out = LLVMDoubleTypeInContext(codegen->ctx);
      break;
    }
    case 128: {
      out = LLVMFP128TypeInContext(codegen->ctx);
      break;
    }
    }
    break;
  }
  case RIRTypeKind::Pointer: {
    out = LLVMPointerType(typeToLLVM(codegen, type->child, ""), 0);
    break;
  }
  case RIRTypeKind::Slice: {
    LLVMTypeRef elem = typeToLLVM(codegen, type->slice.child, "");
    if (type->slice.length > 0) {
      out = LLVMArrayType(elem, type->slice.length);
    } else if (type->slice.length < 0) {
      out = LLVMPointerType(elem, 0);
    } else {
      LLVMTypeRef *types = (LLVMTypeRef *)codegen->allocator->allocZeroed(
          sizeof(LLVMTypeRef) * 2);
      types[0] = LLVMPointerType(elem, 0);
      types[1] = LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size);
      out = LLVMStructTypeInContext(codegen->ctx, types, 2, false);
    }
    break;
  }
  // TODO: case RIRTypeKind::SIMD: {
  //   out = LLVMVectorType(typeToLLVM(codegen, type->slice.type),
  //                        type->slice.length);
  //   break;
  // }
  case RIRTypeKind::TypeId: {
    break;
  }
  case RIRTypeKind::Function: {
    ArrayList<LLVMTypeRef> param_types;
    param_types.init(codegen->allocator, type->function.arguments.len);

    FnABICache abi_cache;

    // Return
    LLVMTypeRef ir_ret_type = typeToLLVM(codegen, type->function._return);
    abi_cache.return_arg =
        codegen->target_abi.classifyReturnType(codegen->mod, ir_ret_type);

    LLVMTypeRef return_type = LLVMVoidTypeInContext(codegen->ctx);
    if (abi_cache.return_arg.kind == ABIArgKind::Direct) {
      return_type = abi_cache.return_arg.type;
    } else if (abi_cache.return_arg.kind == ABIArgKind::Indirect) {
      param_types.push(LLVMPointerType(abi_cache.return_arg.type, 0));
    }

    // Parameters
    abi_cache.args.len = type->function.arguments.len;
    abi_cache.args.ptr = (ABIArg *)codegen->allocator->allocZeroed(
        sizeof(ABIArg) * abi_cache.args.len);

    for (size_t i = 0; i < type->function.arguments.len; i++) {
      RIRTypeId arg_type = type->function.arguments.ptr[i];
      LLVMTypeRef ir_arg_type = typeToLLVM(codegen, arg_type);
      ABIArg arg =
          codegen->target_abi.classifyArgumentType(codegen->mod, ir_arg_type);
      abi_cache.args[i] = arg;

      if (arg.kind == ABIArgKind::Direct) {
        param_types.push(arg.type);
      } else if (arg.kind == ABIArgKind::Indirect) {
        param_types.push(LLVMPointerType(arg.type, 0));
      }
    }

    out = LLVMFunctionType(return_type, param_types.data.ptr,
                           param_types.length, false);
    codegen->fn_abi_cache.insert(out, abi_cache);
    break;
  }
  case RIRTypeKind::Struct: {
    LLVMTypeRef *field_types = (LLVMTypeRef *)codegen->allocator->allocZeroed(
        sizeof(LLVMTypeRef) * type->_struct.fields.len);
    for (size_t i = 0; i < type->_struct.fields.len; i++) {
      field_types[i] = typeToLLVM(codegen, type->_struct.fields.ptr[i]);
    }

    out = LLVMStructCreateNamed(codegen->ctx, name);
    LLVMStructSetBody(out, field_types, type->_struct.fields.len, false);
    break;
  }
  case RIRTypeKind::Enum: {
    out = typeToLLVM(codegen, type->_enum.repr);
    break;
  }
  case RIRTypeKind::Union: {
    // Raw/C-Style Union
    RIRType *repr_type = codegen->rir_context->getType(type->_union.repr);
    if (repr_type->kind == RIRTypeKind::Void) {
      LLVMTypeRef ty = LLVMArrayType(
          LLVMInt8TypeInContext(codegen->ctx),
          type->sizeBits(codegen->rir_context->types, codegen->pointer_size));
      out = LLVMStructTypeInContext(codegen->ctx, &ty, 1, false);
    } else {
      size_t data_size =
          type->sizeBits(codegen->rir_context->types, codegen->pointer_size) -
          repr_type->integer.bits;
      data_size = (data_size + 7) / 8; // Bits to Bytes

      LLVMTypeRef tys[2];
      tys[0] = LLVMIntTypeInContext(codegen->ctx, repr_type->integer.bits);
      tys[1] = LLVMArrayType(LLVMInt8TypeInContext(codegen->ctx), data_size);
      out = LLVMStructTypeInContext(codegen->ctx, tys, 2, false);
    }
    break;
  }
  }

  codegen->type_to_llvm.insert(type_id, out);
  return out;
}

LLVMValueRef constantToLLVM(CodeGenModule *codegen, RIRConstant *constant,
                            RIRTypeId const_type) {
  switch (constant->kind) {
  case RIRConstantKind::Bool: {
    return LLVMConstInt(LLVMInt1TypeInContext(codegen->ctx), constant->_bool,
                        false);
  }
  case RIRConstantKind::Integer: {
    RIRType *result_type = codegen->rir_context->getType(const_type);
    LLVMTypeRef type = typeToLLVM(codegen, const_type);
    return LLVMConstInt(type, constant->integer,
                        result_type->integer.is_signed);
  }
  case RIRConstantKind::Float: {
    LLVMTypeRef type = typeToLLVM(codegen, const_type);
    return LLVMConstReal(type, constant->_float);
  }
  case RIRConstantKind::List: {
    LLVMTypeRef llvm_type = typeToLLVM(codegen, const_type);
    RIRType *list_type = codegen->rir_context->getType(const_type);

    LLVMValueRef aggregate = LLVMConstNull(llvm_type);
    for (size_t i = 0; i < constant->constants.len; i++) {
      RIRTypeId elem_type;
      if (list_type->kind == RIRTypeKind::Slice) {
        elem_type = list_type->slice.child;
      } else if (list_type->kind == RIRTypeKind::Struct) {
        elem_type = list_type->_struct.fields[i];
      }

      RIRConstant *elem = constant->constants.ptr + i;
      LLVMValueRef elem_val = constantToLLVM(codegen, elem, elem_type);
      aggregate =
          LLVMBuildInsertValue(codegen->builder, aggregate, elem_val, i, "");
    }

    if (list_type->kind == RIRTypeKind::Slice && list_type->slice.length == 0) {
      LLVMTypeRef elem_type = typeToLLVM(codegen, list_type->slice.child);
      LLVMValueRef ptr = LLVMAddGlobal(
          codegen->mod, LLVMArrayType(elem_type, constant->constants.len),
          "const_ptr");
      LLVMSetGlobalConstant(ptr, true);
      LLVMSetLinkage(ptr, LLVMPrivateLinkage);
      LLVMSetInitializer(ptr, aggregate);

      LLVMValueRef slice_out[2];
      slice_out[0] = ptr;
      slice_out[1] = LLVMConstInt(
          LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size),
          constant->constants.len, false);
      aggregate = LLVMConstStructInContext(codegen->ctx, slice_out, 2, false);
    }

    return aggregate;
  }
  }

  return nullptr;
}
