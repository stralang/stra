#include "codegen.hpp"
#include "print.hpp"
#include "types.hpp"
#include "llvm-c/Core.h"
#include "llvm-c/Target.h"
#include "llvm-c/Types.h"
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <iostream>

LLVMTypeRef typeToLLVM(CodeGen *codegen, Type *type) {
  switch (type->kind) {
  case TypeKind::Void: {
    return LLVMVoidType();
  }
  case TypeKind::Bool: {
    return LLVMInt1Type();
  }
  case TypeKind::Integer: {
    size_t bits = type->integer.bits;
    if (type->integer.bits == -1) {
      LLVMTypeRef tmp_type = LLVMPointerType(LLVMInt1Type(), 0);
      bits =
          LLVMSizeOfTypeInBits(LLVMGetModuleDataLayout(codegen->mod), tmp_type);
    }
    return LLVMIntType(bits);
  }
  case TypeKind::Float: {
    switch (type->_float.bits) {
    case 16: {
      return LLVMHalfType();
    }
    case 32: {
      return LLVMFloatType();
    }
    case 64: {
      return LLVMDoubleType();
    }
    case 128: {
      return LLVMFP128Type();
    }
    }

    return nullptr;
  }
  case TypeKind::Pointer: {
    return LLVMPointerType(typeToLLVM(codegen, type->child), 0);
  }
  case TypeKind::Slice: {
    LLVMTypeRef elem = typeToLLVM(codegen, type->slice.type);
    if (type->slice.length > 0) {
      return LLVMArrayType(elem, type->slice.length);
    } else if (type->slice.length < 0) {
      return LLVMPointerType(elem, 0);
    } else {
      LLVMTypeRef *types =
          (LLVMTypeRef *)codegen->allocator->alloc(sizeof(LLVMTypeRef) * 2);
      types[0] = LLVMPointerType(elem, 0);
      size_t bits =
          LLVMSizeOfTypeInBits(LLVMGetModuleDataLayout(codegen->mod), types[0]);
      types[1] = LLVMIntType(bits);
      return LLVMStructType(types, 2, false);
    }
    return nullptr;
  }
  case TypeKind::SIMD: {
    return LLVMVectorType(typeToLLVM(codegen, type->slice.type),
                          type->slice.length);
  }
  case TypeKind::TypeId: {
    return nullptr;
  }
  case TypeKind::Function: {
    LLVMTypeRef return_type = typeToLLVM(codegen, type->function.return_type);
    LLVMTypeRef *param_types = (LLVMTypeRef *)codegen->allocator->alloc(
        sizeof(LLVMTypeRef) * type->function.arguments.length);
    for (size_t i = 0; i < type->function.arguments.length; i++) {
      param_types[i] =
          typeToLLVM(codegen, type->function.arguments.data.ptr[i]);
    }

    return LLVMFunctionType(return_type, param_types,
                            type->function.arguments.length, false);
  }
  case TypeKind::Struct: {
    LLVMTypeRef *cache = codegen->scope_to_type.get(type->_struct.scope);
    if (cache != nullptr) {
      return *cache;
    }

    LLVMTypeRef *field_types = (LLVMTypeRef *)codegen->allocator->alloc(
        sizeof(LLVMTypeRef) * type->_struct.fields.length);
    for (size_t i = 0; i < type->_struct.fields.length; i++) {
      field_types[i] = typeToLLVM(codegen, type->_struct.fields.data.ptr[i]);
    }

    LLVMTypeRef ty =
        LLVMStructType(field_types, type->_struct.fields.length, false);
    codegen->scope_to_type.insert(type->_struct.scope, ty);
    return ty;
  }
  case TypeKind::Enum: {
    return typeToLLVM(codegen, type->_enum.repr_type);
  }
  case TypeKind::Union: {
    LLVMTypeRef tmp_ptr = LLVMPointerType(LLVMInt1Type(), 0);
    size_t native_size =
        LLVMSizeOfTypeInBits(LLVMGetModuleDataLayout(codegen->mod), tmp_ptr);

    size_t size = type->sizeBits(native_size);
    return LLVMArrayType(LLVMInt8Type(), size);
  }
  }

  return nullptr;
}

LLVMValueRef valueToLLVM(CodeGen *codegen, Value *value) {
  if (!value->has_data) {
    return nullptr;
  }

  switch (value->type->kind) {
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
  }

  return nullptr;
}

LLVMValueRef gen(CodeGen *codegen, LLVMBuilderRef builder, Node *node,
                 Scope *scope) {
  switch (node->kind) {
  case NodeKind::Compound: {
    Scope *compound_scope = scope->findScope(node);
    if (compound_scope == nullptr) {
      compound_scope = scope;
    }

    for (size_t i = 0; i < node->children.length; i++) {
      gen(codegen, builder, node->children.data.ptr[i], compound_scope);
    }
    break;
  }
  case NodeKind::Name: {
    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    LLVMValueRef value = gen(codegen, builder, symbol->node, symbol->parent);
    // TODO: A variable may not be generated for runtime
    return value;
  }
  // ...
  case NodeKind::Field: {
    LLVMValueRef *cache = codegen->node_to_value.get(node);
    if (cache != nullptr) {
      return *cache;
    }

    // TODO: Mangle name
    char *name = (char *)codegen->allocator->alloc(node->field.name.len);
    memcpy(name, node->field.name.ptr, node->field.name.len);
    name[node->field.name.len] = 0;

    if (node->value.type->kind == TypeKind::Function) {
      // Build function and set name
      LLVMValueRef func = gen(codegen, builder, node->field.initial, scope);
      LLVMSetValueName2(func, (const char *)node->field.name.ptr,
                        node->field.name.len);
      codegen->node_to_value.insert(node, func);
      return func;
    } else if (node->value.type->kind == TypeKind::TypeId) {
      // Type
      Type *real_type = node->value.data.type_value;
      if (real_type->kind == TypeKind::Struct) {
        LLVMTypeRef type = typeToLLVM(codegen, real_type);
        // TODO: Set name
      }

      // Let the type generate it's children
      gen(codegen, builder, node->field.initial, scope);
    } else if (scope->location_aware) {
      // Local Variable
      LLVMTypeRef type = typeToLLVM(codegen, node->value.type);
      LLVMValueRef alloca = LLVMBuildAlloca(builder, type, name);

      if (node->value.has_data) {
        LLVMBuildStore(builder, valueToLLVM(codegen, &node->value), alloca);
      } else if (!node->field.undefined) {
        LLVMValueRef initial =
            gen(codegen, builder, node->field.initial, scope);
        LLVMBuildStore(builder, initial, alloca);
      }

      codegen->node_to_value.insert(node, alloca);
      return alloca;
    } else {
      // Global Variable
      LLVMTypeRef type = typeToLLVM(codegen, node->value.type);
      LLVMValueRef alloca = LLVMAddGlobal(codegen->mod, type, name);
      if (!node->field.undefined) {
        // TODO: Don't set initializer if the variable is outside of the current
        // module
        LLVMSetInitializer(alloca, valueToLLVM(codegen, &node->value));
      }

      codegen->node_to_value.insert(node, alloca);
      return alloca;
    }
    break;
  }
  case NodeKind::Function: {
    LLVMTypeRef type = typeToLLVM(codegen, node->value.type);
    LLVMValueRef func = LLVMAddFunction(codegen->mod, "", type);

    if (!node->function.undefined) {
      // TODO: Don't build body if the function is outside of the current module
      LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func, "entry");
      LLVMBuilderRef body_builder = LLVMCreateBuilder();
      LLVMPositionBuilderAtEnd(body_builder, entry);

      Scope *fn_scope = scope->findScope(node);
      gen(codegen, body_builder, node->function.body, fn_scope);

      if (LLVMGetBasicBlockTerminator(entry) == nullptr) {
        LLVMBuildRetVoid(body_builder);
      }
    }

    return func;
  }
  case NodeKind::Struct: {
    Scope *struct_scope = scope->findScope(node);
    for (size_t i = 0; i < node->_struct.body.length; i++) {
      gen(codegen, builder, node->_struct.body.data.ptr[i], struct_scope);
    }
    break;
  }
  case NodeKind::Enum: {
    Scope *enum_scope = scope->findScope(node);
    for (size_t i = 0; i < node->_enum.body.length; i++) {
      gen(codegen, builder, node->_enum.body.data.ptr[i], enum_scope);
    }
    break;
  }
  case NodeKind::Union: {
    Scope *union_scope = scope->findScope(node);
    for (size_t i = 0; i < node->_union.body.length; i++) {
      gen(codegen, builder, node->_union.body.data.ptr[i], union_scope);
    }
    break;
  }
  }

  return nullptr;
}

void CodeGen::generate() {
  char *name = (char *)allocator->alloc(sizeof(this->path.len + 1));
  memcpy(name, this->path.ptr, this->path.len);
  *(name + this->path.len) = 0;

  this->mod = LLVMModuleCreateWithName(name);

  this->scope_to_type.init(this->allocator, 32);
  this->node_to_value.init(this->allocator, 32);

  gen(this, nullptr, this->ast, this->scope);

  char *text = LLVMPrintModuleToString(this->mod);
  std::cout << text;
}
