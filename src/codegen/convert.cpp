#include "codegen.hpp"
#include "define.hpp"
#include "llvm-c/DebugInfo.h"
#include "llvm-c/Types.h"
#include <cstdlib>
#include <cstring>
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

    return LLVMConstArray(elem_type, values, value->data.text.len);
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

LLVMMetadataRef typeToLLVMDebug(CodeGenModule *codegen, Type *type,
                                const char *name) {
  LLVMMetadataRef out;
  switch (type->kind) {
  case TypeKind::Void: {
    out = LLVMDIBuilderCreateBasicType(codegen->dbg_builder, "void", 4, 0, 0,
                                       LLVMDIFlagZero);
    break;
  }
  case TypeKind::Bool: {
    out = LLVMDIBuilderCreateBasicType(codegen->dbg_builder, "bool", 3, 1, 0x2,
                                       LLVMDIFlagZero);
    break;
  }
  case TypeKind::Integer: {
    const char *name;
    if (type->integer.bits == -1) {
      name = "native int";
    } else {
      name = "int";
    }

    LLVMDWARFTypeEncoding encoding = 0x5;
    if (!type->integer.is_signed) {
      encoding = 0x7;
    }

    size_t name_len = strlen(name);
    out = LLVMDIBuilderCreateBasicType(codegen->dbg_builder, name, name_len,
                                       type->integer.bits, encoding,
                                       LLVMDIFlagZero);
    break;
  }
  case TypeKind::Float: {
    out = LLVMDIBuilderCreateBasicType(codegen->dbg_builder, "float", 5,
                                       type->_float.bits, 0x4, LLVMDIFlagZero);
    break;
  }
  case TypeKind::Pointer: {
    out = LLVMDIBuilderCreateBasicType(codegen->dbg_builder, "pointer", 7,
                                       codegen->pointer_size, 0x1,
                                       LLVMDIFlagZero);
    break;
  }
  case TypeKind::Slice: {
    if (type->slice.length > 0) {
      size_t align = type->alignBits(codegen->pointer_size);
      LLVMMetadataRef meta_ty = typeToLLVMDebug(codegen, type->slice.type);
      out = LLVMDIBuilderCreateArrayType(
          codegen->dbg_builder, type->slice.length, align, meta_ty, nullptr, 0);
    } else if (type->slice.length == 0) {
      LLVMMetadataRef elements[2];
      elements[0] = LLVMDIBuilderCreateBasicType(codegen->dbg_builder, "ptr", 3,
                                                 codegen->pointer_size, 0x1,
                                                 LLVMDIFlagZero);
      elements[1] = LLVMDIBuilderCreateBasicType(codegen->dbg_builder, "len", 3,
                                                 codegen->pointer_size, 0x7,
                                                 LLVMDIFlagZero);

      out = LLVMDIBuilderCreateStructType(
          codegen->dbg_builder, codegen->dbg_scope, "slice", 5,
          codegen->dbg_file, 0, codegen->pointer_size * 2,
          codegen->pointer_size, LLVMDIFlagZero, nullptr, elements, 2, 0,
          nullptr, "", 0);
    } else if (type->slice.length < 0) {
      out = LLVMDIBuilderCreateBasicType(codegen->dbg_builder, "pointer slice",
                                         13, codegen->pointer_size, 0x1,
                                         LLVMDIFlagZero);
    }
    break;
  }
  case TypeKind::SIMD: {
    size_t size = type->sizeBits(codegen->pointer_size);
    size_t align = type->alignBits(codegen->pointer_size);
    LLVMMetadataRef meta_ty = typeToLLVMDebug(codegen, type->slice.type);
    out = LLVMDIBuilderCreateVectorType(codegen->dbg_builder, size, align,
                                        meta_ty, nullptr, 0);
    break;
  }
  case TypeKind::TypeId: {
    break;
  }
  case TypeKind::Function: {
    LLVMMetadataRef *parameters = (LLVMMetadataRef *)malloc(
        sizeof(LLVMMetadataRef) * type->function.arguments.length);
    for (size_t i = 0; i < type->function.arguments.length; i++) {
      parameters[i] =
          typeToLLVMDebug(codegen, type->function.arguments.data.ptr[i]);
    }

    out = LLVMDIBuilderCreateSubroutineType(
        codegen->dbg_builder, codegen->dbg_file, parameters,
        type->function.arguments.length, LLVMDIFlagZero);
    break;
  }
  case TypeKind::Struct: {
    Symbol *symbol = type->_struct.scope;
    size_t size = type->sizeBits(codegen->pointer_size);
    size_t align = type->alignBits(codegen->pointer_size);

    LLVMMetadataRef *elements = (LLVMMetadataRef *)malloc(
        sizeof(LLVMMetadataRef) * type->_struct.fields.length);
    for (size_t i = 0; i < type->_struct.fields.length; i++) {
      elements[i] = typeToLLVMDebug(codegen, type->_struct.fields.data.ptr[i]);
    }

    out = LLVMDIBuilderCreateStructType(
        codegen->dbg_builder, codegen->dbg_scope, name, strlen(name),
        codegen->dbg_file, symbol->node->location.line, size, align,
        LLVMDIFlagZero, nullptr, elements, type->_struct.fields.length, 0,
        nullptr, "", 0);
    break;
  }
  case TypeKind::Enum: {
    LLVMMetadataRef class_ty = typeToLLVMDebug(codegen, type->_enum.repr_type);

    Symbol *symbol = type->_enum.scope;
    size_t size = type->sizeBits(codegen->pointer_size);
    size_t align = type->alignBits(codegen->pointer_size);

    size_t element_count = symbol->node->_enum.members.length;
    LLVMMetadataRef *elements =
        (LLVMMetadataRef *)malloc(sizeof(LLVMMetadataRef) * element_count);
    for (size_t i = 0; i < element_count; i++) {
      Node *member = symbol->node->_enum.members.data.ptr[i];
      elements[i] = LLVMDIBuilderCreateEnumerator(
          codegen->dbg_builder, (const char *)member->member.name.ptr,
          member->member.name.len, member->value.data.integer,
          !type->_enum.repr_type->_enum.repr_type->integer.is_signed);
    }

    out = LLVMDIBuilderCreateEnumerationType(
        codegen->dbg_builder, codegen->dbg_scope, name, strlen(name),
        codegen->dbg_file, symbol->node->location.line, size, align, elements,
        element_count, class_ty);
    break;
  }
  case TypeKind::Union: {
    Symbol *symbol = type->_union.scope;
    size_t size = type->sizeBits(codegen->pointer_size);
    size_t align = type->alignBits(codegen->pointer_size);

    size_t element_count = type->_union.variants.length;
    LLVMMetadataRef *elements =
        (LLVMMetadataRef *)malloc(sizeof(LLVMMetadataRef) * element_count);
    for (size_t i = 0; i < element_count; i++) {
      elements[i] = typeToLLVMDebug(codegen, type->_union.variants.data.ptr[i]);
    }

    out = LLVMDIBuilderCreateUnionType(
        codegen->dbg_builder, codegen->dbg_scope, name, strlen(name),
        codegen->dbg_file, symbol->node->location.line, size, align,
        LLVMDIFlagZero, elements, element_count, 0, "", 0);
    break;
  }
  }

  return out;
}
