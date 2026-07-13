#include "codegen.hpp"
#include "define.hpp"
#include "llvm-c/Types.h"
#include <llvm-c/Core.h>

LLVMTypeRef typeToLLVM(CodeGenModule *codegen, Type *type, const char *name) {
  LLVMTypeRef *type_cache = codegen->type_to_llvm.get(type);
  if (type_cache != nullptr) {
    return *type_cache;
  }

  LLVMTypeRef out;
  switch (type->kind) {
  case TypeKind::Void: {
    out = LLVMVoidTypeInContext(codegen->ctx);
    break;
  }
  case TypeKind::Bool: {
    out = LLVMInt1TypeInContext(codegen->ctx);
    break;
  }
  case TypeKind::Integer: {
    size_t bits = type->integer.bits;
    if (type->integer.bits == -1) {
      bits = codegen->pointer_size;
    }
    out = LLVMIntTypeInContext(codegen->ctx, bits);
    break;
  }
  case TypeKind::Float: {
    switch (type->_float.bits) {
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
  case TypeKind::Pointer: {
    out = LLVMPointerType(typeToLLVM(codegen, type->child, ""), 0);
    break;
  }
  case TypeKind::Slice: {
    LLVMTypeRef elem = typeToLLVM(codegen, type->slice.type, "");
    if (type->slice.length > 0) {
      out = LLVMArrayType(elem, type->slice.length);
    } else if (type->slice.length < 0) {
      out = LLVMPointerType(elem, 0);
    } else {
      LLVMTypeRef *types =
          (LLVMTypeRef *)codegen->allocator->alloc(sizeof(LLVMTypeRef) * 2);
      types[0] = LLVMPointerType(elem, 0);
      types[1] = LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size);
      out = LLVMStructTypeInContext(codegen->ctx, types, 2, false);
    }
    break;
  }
  case TypeKind::SIMD: {
    out = LLVMVectorType(typeToLLVM(codegen, type->slice.type),
                         type->slice.length);
    break;
  }
  case TypeKind::TypeId: {
    break;
  }
  case TypeKind::Function: {
    ArrayList<LLVMTypeRef> param_types;
    param_types.init(codegen->allocator, type->function.arguments.length);

    FnABICache abi_cache;

    // Return
    LLVMTypeRef ir_ret_type = typeToLLVM(codegen, type->function.return_type);
    abi_cache.return_arg =
        codegen->target_abi.classifyReturnType(codegen->mod, ir_ret_type);

    LLVMTypeRef return_type = LLVMVoidTypeInContext(codegen->ctx);
    if (abi_cache.return_arg.kind == ABIArgKind::Direct) {
      return_type = abi_cache.return_arg.type;
    } else if (abi_cache.return_arg.kind == ABIArgKind::Indirect) {
      param_types.push(LLVMPointerType(abi_cache.return_arg.type, 0));
    }

    // Parameters
    abi_cache.args.len = type->function.arguments.length;
    abi_cache.args.ptr = (ABIArg *)codegen->allocator->alloc(
        sizeof(ABIArg) * abi_cache.args.len);

    for (size_t i = 0; i < type->function.arguments.length; i++) {
      Type *arg_type = type->function.arguments.data.ptr[i];
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
  case TypeKind::Struct: {
    LLVMTypeRef *field_types = (LLVMTypeRef *)codegen->allocator->alloc(
        sizeof(LLVMTypeRef) * type->_struct.fields.length);
    for (size_t i = 0; i < type->_struct.fields.length; i++) {
      field_types[i] = typeToLLVM(codegen, type->_struct.fields.data.ptr[i]);
    }

    out = LLVMStructCreateNamed(codegen->ctx, name);
    LLVMStructSetBody(out, field_types, type->_struct.fields.length, false);
    break;
  }
  case TypeKind::Enum: {
    out = typeToLLVM(codegen, type->_enum.repr_type);
    break;
  }
  case TypeKind::Union: {
    // Raw/C-Style Union
    if (type->_union.repr_type->kind == TypeKind::Void) {
      LLVMTypeRef ty = LLVMArrayType(LLVMInt8TypeInContext(codegen->ctx),
                                     type->sizeBits(codegen->pointer_size));
      out = LLVMStructTypeInContext(codegen->ctx, &ty, 1, false);
    } else {
      size_t data_size = type->sizeBits(codegen->pointer_size) -
                         type->_union.repr_type->integer.bits;
      data_size = (data_size + 7) / 8; // Bits to Bytes

      LLVMTypeRef tys[2];
      tys[0] = LLVMIntTypeInContext(codegen->ctx,
                                    type->_union.repr_type->integer.bits);
      tys[1] = LLVMArrayType(LLVMInt8TypeInContext(codegen->ctx), data_size);
      out = LLVMStructTypeInContext(codegen->ctx, tys, 2, false);
    }
    break;
  }
  }

  codegen->type_to_llvm.insert(type, out);
  return out;
}

LLVMValueRef valueToLLVM(CodeGenModule *codegen, Value *value) {
  if (!value->has_data) {
    return nullptr;
  }

  switch (value->type->kind) {
  case TypeKind::Bool: {
    return LLVMConstInt(LLVMInt1TypeInContext(codegen->ctx), value->data._bool,
                        false);
  }
  case TypeKind::Integer: {
    LLVMTypeRef type = typeToLLVM(codegen, value->type);
    return LLVMConstInt(type, value->data.integer,
                        value->type->integer.is_signed);
  }
  case TypeKind::Float: {
    LLVMTypeRef type = typeToLLVM(codegen, value->type);
    return LLVMConstReal(type, value->data._float);
  }
  case TypeKind::Slice: {
    LLVMTypeRef elem_type = typeToLLVM(codegen, value->type->slice.type);
    LLVMValueRef *values = (LLVMValueRef *)codegen->allocator->alloc(
        sizeof(LLVMValueRef) * value->data.text.len);

    for (size_t i = 0; i < value->data.text.len; i++) {
      values[i] = LLVMConstInt(elem_type, value->data.text.ptr[i], false);
    }

    LLVMValueRef data = LLVMConstArray(elem_type, values, value->data.text.len);

    // Runtime
    if (value->type->slice.length == 0) {
      LLVMValueRef ptr = LLVMAddGlobal(
          codegen->mod, LLVMArrayType(elem_type, value->data.text.len),
          "const_ptr");
      LLVMSetGlobalConstant(ptr, true);
      LLVMSetLinkage(ptr, LLVMPrivateLinkage);
      LLVMSetInitializer(ptr, data);

      LLVMValueRef slice_out[2];
      slice_out[0] = ptr;
      slice_out[1] = LLVMConstInt(
          LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size),
          value->data.text.len, false);
      data = LLVMConstStructInContext(codegen->ctx, slice_out, 2, false);
    }

    return data;
  }
  case TypeKind::Enum: {
    Type *repr_type = value->type->_enum.repr_type;
    LLVMTypeRef type = typeToLLVM(codegen, repr_type);
    return LLVMConstInt(type, value->data.integer,
                        repr_type->integer.is_signed);
  }
  }

  return nullptr;
}
