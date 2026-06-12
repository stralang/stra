#include "codegen.hpp"
#include "abi/general.hpp"
#include "ast.hpp"
#include "containers.hpp"
#include "operator.hpp"
#include "print.hpp"
#include "symbol.hpp"
#include "types.hpp"
#include "llvm-c/Core.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Transforms/PassBuilder.h"
#include "llvm-c/Types.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <llvm-c/TargetMachine.h>
#include <sstream>

// Forward Declaration
LLVMValueRef gen(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                 Symbol *scope);
LLVMValueRef addr(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                  Symbol *scope);

LLVMTypeRef typeToLLVM(CodeGenModule *codegen, Type *type,
                       const char *name = nullptr) {
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
  }

  return nullptr;
}

LLVMValueRef genMemberAccess(CodeGenModule *codegen, LLVMBuilderRef builder,
                             Node *node, Symbol *scope) {
  Node *lhs = node->_operator.lhs;
  Type *lhs_type = lhs->value.type;

  ArrayList<Node *> *impl_children = nullptr;
  Symbol *impl_scope = nullptr;

  // Auto Dereference
  bool auto_dereference = false;
  if (lhs_type->kind == TypeKind::Pointer) {
    lhs_type = lhs_type->child;
    auto_dereference = true;
  }

  // Access
  if (lhs_type->kind == TypeKind::Struct) {
    Symbol *struct_symbol = lhs_type->_struct.scope;
    Node *struct_node = struct_symbol->node;

    // Child stuff
    impl_children = &struct_node->_struct.body;
    impl_scope = struct_symbol;

    // Find Field
    size_t field_idx = SIZE_MAX;
    for (size_t i = 0; i < struct_node->_struct.fields.length; i++) {
      Node *field = struct_node->_struct.fields.data.ptr[i];
      if (field->field.name.compare(node->_operator.rhs->text)) {
        field_idx = i;
        break;
      }
    }

    // Get element
    if (field_idx != SIZE_MAX) {
      LLVMValueRef value;
      if (auto_dereference) {
        value = gen(codegen, builder, lhs, scope);
      } else {
        value = addr(codegen, builder, lhs, scope);
      }

      LLVMValueRef indices[2];
      LLVMTypeRef index_ty = LLVMInt32TypeInContext(codegen->ctx);
      indices[0] = LLVMConstInt(index_ty, 0, false);
      indices[1] = LLVMConstInt(index_ty, field_idx, false);
      return LLVMBuildGEP2(builder, typeToLLVM(codegen, lhs_type), value,
                           indices, 2, "");
    }
  } else if (lhs->value.type->kind == TypeKind::Enum) {
    Symbol *enum_symbol = lhs_type->_enum.scope;
    impl_children = &enum_symbol->node->_enum.body;
    impl_scope = enum_symbol;
  } else if (lhs->value.type->kind == TypeKind::Union) {
    Symbol *union_symbol = lhs_type->_union.scope;
    Node *union_node = union_symbol->node;

    // Child stuff
    impl_children = &union_node->_union.body;
    impl_scope = union_symbol;

    // Find Type
    size_t variant_idx = SIZE_MAX;
    for (size_t i = 0; i < union_node->_union.variants.length; i++) {
      Node *variant = union_node->_union.variants.data.ptr[i];
      if (variant->field.name.compare(node->_operator.rhs->text)) {
        variant_idx = i;
        break;
      }
    }

    // Cast
    if (variant_idx != SIZE_MAX) {
      // Get data field
      LLVMValueRef value;
      if (auto_dereference) {
        value = gen(codegen, builder, lhs, scope);
      } else {
        value = addr(codegen, builder, lhs, scope);
      }

      LLVMValueRef indices[2];
      LLVMTypeRef index_ty = LLVMInt32TypeInContext(codegen->ctx);
      indices[0] = LLVMConstInt(index_ty, 0, false);

      // Check Tag
      size_t data_offset = 0;
      if (lhs->value.type->_union.repr_type->kind != TypeKind::Void) {
        data_offset += 1;
        indices[1] = indices[0];
        LLVMValueRef tag_ptr =
            LLVMBuildGEP2(builder, typeToLLVM(codegen, lhs->value.type), value,
                          indices, 2, "");
        LLVMTypeRef repr_type =
            typeToLLVM(codegen, lhs->value.type->_union.repr_type);
        LLVMValueRef runtime_tag =
            LLVMBuildLoad2(builder, repr_type, tag_ptr, "");

        LLVMValueRef func =
            codegen->function_stack[codegen->function_stack_len - 1].def;
        LLVMBasicBlockRef fail =
            LLVMAppendBasicBlockInContext(codegen->ctx, func, "tag_check_fail");
        LLVMBasicBlockRef success = LLVMAppendBasicBlockInContext(
            codegen->ctx, func, "tag_check_success");

        // Condition
        LLVMValueRef grab_tag =
            LLVMConstInt(repr_type, variant_idx,
                         lhs->value.type->_union.repr_type->integer.is_signed);
        LLVMValueRef is_valid =
            LLVMBuildICmp(builder, LLVMIntEQ, runtime_tag, grab_tag, "");
        LLVMBuildCondBr(builder, is_valid, success, fail);

        // Fail
        LLVMPositionBuilderAtEnd(builder, fail);
        LLVMBuildUnreachable(builder); // FIXME: there is no panic handling yet

        // Success
        LLVMPositionBuilderAtEnd(builder, success);
      }

      // Get Data
      indices[1] = LLVMConstInt(index_ty, data_offset, false);
      LLVMValueRef data_ptr = LLVMBuildGEP2(
          builder, typeToLLVM(codegen, lhs->value.type), value, indices, 2, "");

      // Cast
      Type *union_type = union_node->value.data.type_value;
      LLVMTypeRef dest_ty = typeToLLVM(
          codegen, union_type->_union.variants.data.ptr[variant_idx]);
      return LLVMBuildBitCast(builder, data_ptr, LLVMPointerType(dest_ty, 0),
                              "");
    }
  } else if (lhs->value.type->kind == TypeKind::TypeId) {
    Type *real_ty = lhs->value.data.type_value;
    if (real_ty->kind == TypeKind::Struct) {
      Symbol *struct_symbol = real_ty->_struct.scope;
      impl_children = &struct_symbol->node->_struct.body;
      impl_scope = struct_symbol;
    } else if (real_ty->kind == TypeKind::Enum) {
      Symbol *enum_symbol = real_ty->_enum.scope;
      impl_children = &enum_symbol->node->_enum.body;
      impl_scope = enum_symbol;
    } else if (real_ty->kind == TypeKind::Union) {
      Symbol *union_symbol = real_ty->_union.scope;
      impl_children = &union_symbol->node->_union.body;
      impl_scope = union_symbol;
    } else if (real_ty->kind == TypeKind::Namespace) {
      Symbol *namespace_symbol = real_ty->_namespace.scope;
      impl_children = &namespace_symbol->node->children;
      impl_scope = namespace_symbol;
    }
  }

  // Find and generate child
  if (impl_children != nullptr) {
    for (size_t i = 0; i < impl_children->length; i++) {
      Node *body = impl_children->data.ptr[i];
      if (!body->field.name.compare(node->_operator.rhs->text)) {
        continue;
      }

      return addr(codegen, builder, node->_operator.rhs, impl_scope);
    }
  }

  return nullptr;
}

LLVMValueRef genUnary(CodeGenModule *codegen, LLVMBuilderRef builder,
                      Node *node, Symbol *scope) {
  Type *child_type = node->unary_operator.child->value.type;
  if (child_type->kind == TypeKind::SIMD) {
    child_type = child_type->slice.type;
  }

  if (node->unary_operator.opcode == UnaryOperator::Reference) {
    return addr(codegen, builder, node->unary_operator.child, scope);
  }

  LLVMValueRef value = gen(codegen, builder, node->unary_operator.child, scope);

  switch (node->unary_operator.opcode) {
  case UnaryOperator::Minus: {
    if (child_type->kind == TypeKind::Integer) {
      return LLVMBuildNeg(builder, value, "");
    } else if (child_type->kind == TypeKind::Float) {
      return LLVMBuildFNeg(builder, value, "");
    }

    break;
  }
  case UnaryOperator::Logical_Not: {
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
  case UnaryOperator::Bitwise_Not: {
    if (child_type->kind == TypeKind::Integer) {
      return LLVMBuildNot(builder, value, "");
    }
    break;
  }
  case UnaryOperator::Dereference: {
    return LLVMBuildLoad2(builder, typeToLLVM(codegen, child_type->child),
                          value, "");
  }
  }

  return nullptr;
}

LLVMValueRef genBinary(CodeGenModule *codegen, LLVMBuilderRef builder,
                       Node *node, Symbol *scope) {
  // Member Access
  if (node->_operator.opcode == Operator::MemberAccess) {
    // Get enum value
    Value *lhs_value = &node->_operator.lhs->value;
    if (lhs_value->type->kind == TypeKind::Enum ||
        (lhs_value->type->kind == TypeKind::TypeId &&
         lhs_value->data.type_value->kind == TypeKind::Enum)) {
      Type *real_ty = lhs_value->type;
      if (lhs_value->type->kind == TypeKind::TypeId) {
        real_ty = lhs_value->data.type_value;
      }

      int64_t value = node->value.data.integer;
      Type *repr_ty = real_ty->_enum.repr_type;
      return LLVMConstInt(typeToLLVM(codegen, repr_ty), value,
                          repr_ty->integer.is_signed);
    }

    LLVMValueRef ptr = genMemberAccess(codegen, builder, node, scope);
    return LLVMBuildLoad2(builder, typeToLLVM(codegen, node->value.type), ptr,
                          "");
  }

  Type *lhs_type = node->_operator.lhs->value.type;
  Type *rhs_type = node->_operator.rhs->value.type;

  // Assignment
  if (node->_operator.opcode == Operator::Assign) {
    LLVMValueRef rhs_value = gen(codegen, builder, node->_operator.rhs, scope);
    LLVMValueRef lhs_ptr = addr(codegen, builder, node->_operator.lhs, scope);

    if (node->_operator.lhs->value.type->kind == TypeKind::Union) {
      Type *union_type = node->_operator.lhs->value.type;

      // Get tag id
      size_t tag_id = 0;
      Type *var_type = nullptr;
      for (size_t i = 0; i < union_type->_union.variants.length; i++) {
        var_type = union_type->_union.variants.data.ptr[i];
        if (node->_operator.rhs->value.type == var_type) {
          tag_id = i;
          break;
        }
      }

      LLVMValueRef indices[2];
      LLVMTypeRef index_ty = LLVMInt32TypeInContext(codegen->ctx);
      indices[0] = LLVMConstInt(index_ty, 0, false);

      size_t data_offset = 0;
      if (node->_operator.lhs->value.type->_union.repr_type->kind !=
          TypeKind::Void) {
        data_offset += 1;

        // Set Tag
        indices[1] = indices[0];
        LLVMValueRef tag_ptr = LLVMBuildGEP2(
            builder, typeToLLVM(codegen, union_type), lhs_ptr, indices, 2, "");

        LLVMTypeRef repr_type =
            typeToLLVM(codegen, union_type->_union.repr_type);
        LLVMValueRef tag_const = LLVMConstInt(
            repr_type, tag_id, union_type->_union.repr_type->integer.is_signed);
        LLVMBuildStore(builder, tag_const, tag_ptr);
      }

      // Set Value
      indices[1] = LLVMConstInt(index_ty, data_offset, false);
      LLVMValueRef data_ptr = LLVMBuildGEP2(
          builder, typeToLLVM(codegen, union_type), lhs_ptr, indices, 2, "");
      LLVMBuildStore(builder, rhs_value, data_ptr);
    } else {
      LLVMBuildStore(builder, rhs_value, lhs_ptr);
    }

    return nullptr;
  }

  LLVMValueRef lhs_value = gen(codegen, builder, node->_operator.lhs, scope);

  if (lhs_type->kind == TypeKind::SIMD) {
    lhs_type = lhs_type->child;
  }

  // Cast
  if (node->_operator.opcode == Operator::As) {
    LLVMTypeRef dest_ty =
        typeToLLVM(codegen, node->_operator.rhs->value.data.type_value);

    if (lhs_type->kind == TypeKind::Integer) {
      return LLVMBuildIntCast2(builder, lhs_value, dest_ty,
                               lhs_type->integer.is_signed, "");
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFPCast(builder, lhs_value, dest_ty, "");
    } else if (lhs_type->kind == TypeKind::Pointer) {
      return LLVMBuildPointerCast(builder, lhs_value, dest_ty, "");
    } else {
      return LLVMBuildBitCast(builder, lhs_value, dest_ty, "");
    }
  } else if (node->_operator.opcode == Operator::Bitcast) {
    LLVMTypeRef dest_ty =
        typeToLLVM(codegen, node->_operator.rhs->value.data.type_value);
    return LLVMBuildBitCast(builder, lhs_value, dest_ty, "");
  }

  LLVMValueRef rhs_value = gen(codegen, builder, node->_operator.rhs, scope);

  switch (node->_operator.opcode) {
  case Operator::Add: {
    if (lhs_type->kind == TypeKind::Integer ||
        lhs_type->kind == TypeKind::Pointer) {
      return LLVMBuildAdd(builder, lhs_value, rhs_value, "");
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFAdd(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case Operator::Sub: {
    if (lhs_type->kind == TypeKind::Integer ||
        lhs_type->kind == TypeKind::Pointer) {
      return LLVMBuildSub(builder, lhs_value, rhs_value, "");
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFSub(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case Operator::Mul: {
    if (lhs_type->kind == TypeKind::Integer) {
      return LLVMBuildMul(builder, lhs_value, rhs_value, "");
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFMul(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case Operator::Div: {
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
  case Operator::Mod: {
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
  case Operator::Bitwise_Or: {
    if (lhs_type->kind == TypeKind::Bool ||
        lhs_type->kind == TypeKind::Integer) {
      return LLVMBuildOr(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case Operator::Bitwise_Xor: {
    if (lhs_type->kind == TypeKind::Bool ||
        lhs_type->kind == TypeKind::Integer) {
      return LLVMBuildXor(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case Operator::Bitwise_And: {
    if (lhs_type->kind == TypeKind::Bool ||
        lhs_type->kind == TypeKind::Integer) {
      return LLVMBuildAnd(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case Operator::Bitwise_LeftShift: {
    if (lhs_type->kind == TypeKind::Integer) {
      return LLVMBuildShl(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case Operator::Bitwise_RightShift: {
    if (lhs_type->kind == TypeKind::Integer) {
      return LLVMBuildLShr(builder, lhs_value, rhs_value, "");
    }
    break;
  }
  case Operator::Logical_Or: {
    return LLVMBuildOr(builder, lhs_value, rhs_value, "");
  }
  case Operator::Logical_And: {
    return LLVMBuildAnd(builder, lhs_value, rhs_value, "");
  }
  case Operator::EqualTo: {
    if (lhs_type->kind == TypeKind::Bool ||
        lhs_type->kind == TypeKind::Integer ||
        lhs_type->kind == TypeKind::Pointer) {
      return LLVMBuildICmp(builder, LLVMIntEQ, lhs_value, rhs_value, "");
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFCmp(builder, LLVMRealOEQ, lhs_value, rhs_value, "");
    }
    break;
  }
  case Operator::NotEqualTo: {
    if (lhs_type->kind == TypeKind::Bool ||
        lhs_type->kind == TypeKind::Integer ||
        lhs_type->kind == TypeKind::Pointer) {
      return LLVMBuildICmp(builder, LLVMIntNE, lhs_value, rhs_value, "");
    } else if (lhs_type->kind == TypeKind::Float) {
      return LLVMBuildFCmp(builder, LLVMRealONE, lhs_value, rhs_value, "");
    }
    break;
  }
  case Operator::LessThen: {
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
  case Operator::GreaterThen: {
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
  case Operator::LessThenOrEqualTo: {
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
  case Operator::GreaterThenOrEqualTo: {
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

void genAssembly(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                 Symbol *scope) {
  ArrayList<LLVMValueRef> inputs;
  ArrayList<LLVMTypeRef> input_types;
  ArrayList<LLVMValueRef> outputs;
  ArrayList<LLVMTypeRef> output_types;
  std::ostringstream assembly;
  std::ostringstream return_constaints;
  std::ostringstream read_constraints;
  std::ostringstream clobbered;

  inputs.init(codegen->allocator, 8);
  input_types.init(codegen->allocator, 8);
  outputs.init(codegen->allocator, 8);
  output_types.init(codegen->allocator, 8);

  // Count
  size_t total_outputs = 0;
  for (size_t i = 0; i < node->assembly.instructions.length; i++) {
    NodeAssembly::Instruction *inst = node->assembly.instructions.data.ptr + i;
    for (size_t a = 0; a < inst->arguments.length; a++) {
      NodeAssembly::Argument *arg = inst->arguments.data.ptr + a;
      if (arg->kind == NodeAssembly::Argument::Return) {
        total_outputs += 1;
      }
    }
  }

  // Convert AST to ASM
  for (size_t i = 0; i < node->assembly.instructions.length; i++) {
    if (i != 0) {
      assembly << "\n";
    }

    NodeAssembly::Instruction *inst = node->assembly.instructions.data.ptr + i;
    assembly << inst->name;

    for (size_t a = 0; a < inst->arguments.length; a++) {
      NodeAssembly::Argument *arg = inst->arguments.data.ptr + a;
      if (a != 0) {
        assembly << ",";
      }

      // Registers
      if (arg->kind == NodeAssembly::Argument::Register) {
        assembly << " %" << arg->reg;
        clobbered << ",~{" << arg->reg << "}";
        continue;
      }

      // Literals
      if (arg->node->kind == NodeKind::Integer) {
        assembly << " $$" << arg->node->value.data.integer;
        continue;
      }

      // I/O
      if (inputs.length != 0) {
        read_constraints << ",";
      }

      LLVMValueRef arg_ptr = addr(codegen, builder, arg->node, scope);
      LLVMTypeRef arg_type = typeToLLVM(codegen, arg->node->value.type);

      if (arg->kind == NodeAssembly::Argument::Return) {
        return_constaints << "=r,";
        read_constraints << "r";
        outputs.push(arg_ptr);
        output_types.push(arg_type);
      } else {
        read_constraints << "r";
      }

      LLVMValueRef arg_value = LLVMBuildLoad2(builder, arg_type, arg_ptr, "");
      inputs.push(arg_value);
      input_types.push(arg_type);
      assembly << " $" << (inputs.length - 1 + total_outputs);
    }
  }

  // Prepare
  return_constaints << read_constraints.str() << clobbered.str();

  std::string assembly_str = assembly.str();
  std::string constraints_str = return_constaints.str();

  // Generate
  LLVMTypeRef call_result = nullptr;
  if (outputs.length == 0) {
    call_result = LLVMVoidTypeInContext(codegen->ctx);
  } else if (outputs.length == 1) {
    call_result = output_types.data[0];
  } else {
    call_result = LLVMStructTypeInContext(codegen->ctx, output_types.data.ptr,
                                          output_types.length, false);
  }

  LLVMTypeRef func_ty =
      LLVMFunctionType(call_result, input_types.data.ptr, inputs.length, false);
  LLVMValueRef inline_asm = LLVMGetInlineAsm(
      func_ty, assembly_str.data(), assembly_str.size(), constraints_str.data(),
      constraints_str.size(), true, true, LLVMInlineAsmDialectATT, false);

  LLVMValueRef asm_result = LLVMBuildCall2(builder, func_ty, inline_asm,
                                           inputs.data.ptr, inputs.length, "");

  // Store Result
  if (outputs.length == 1) {
    LLVMBuildStore(builder, asm_result, outputs.data.ptr[0]);
  } else {
    for (size_t i = 0; i < outputs.length; i++) {
      LLVMValueRef result = LLVMBuildExtractValue(builder, asm_result, i, "");
      LLVMBuildStore(builder, result, outputs.data.ptr[i]);
    }
  }
}

LLVMValueRef addr(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                  Symbol *scope) {
  switch (node->kind) {
  case NodeKind::Name: {
    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    LLVMValueRef *value = codegen->node_to_value.get(symbol->node);
    if (value == nullptr) {
      gen(codegen, builder, symbol->node, symbol->parent);
      value = codegen->node_to_value.get(symbol->node);
    }

    if (value == nullptr) {
      std::cerr << node->location << " Couldn't find value `" << node->text
                << "`. Aborting...\n";
      std::abort();
    }

    // TODO: A variable may not be generated for runtime
    return *value;
  }
  case NodeKind::UnaryOperator: {
    if (node->unary_operator.opcode == UnaryOperator::Dereference) {
      return gen(codegen, builder, node->unary_operator.child, scope);
    }
    break;
  }
  case NodeKind::Operator: {
    if (node->_operator.opcode == Operator::MemberAccess) {
      return genMemberAccess(codegen, builder, node, scope);
    }
    break;
  }
  case NodeKind::Index: {
    LLVMValueRef slice = addr(codegen, builder, node->index.slice, scope);
    Type *slice_type = node->index.slice->value.type;
    LLVMValueRef length = nullptr;
    LLVMValueRef ptr = slice;
    LLVMTypeRef type = nullptr;

    LLVMValueRef indices[2];
    indices[0] = LLVMConstInt(LLVMInt32TypeInContext(codegen->ctx), 0, false);
    if (slice_type->slice.length > 0) {
      // Array (compile-time length)
      type = typeToLLVM(codegen, slice_type->slice.type);

      length = LLVMConstInt(
          LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size),
          slice_type->slice.length, false);
    } else if (slice_type->slice.length == 0) {
      // Slice (runtime length)
      indices[1] = LLVMConstInt(LLVMInt32TypeInContext(codegen->ctx), 1, false);
      length = LLVMBuildGEP2(builder, typeToLLVM(codegen, slice_type), slice,
                             indices, 2, "");
      length = LLVMBuildLoad2(
          builder, LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size),
          length, "");

      indices[1] = indices[0];

      ptr = LLVMBuildGEP2(builder, typeToLLVM(codegen, slice_type), slice,
                          indices, 2, "");
      type = typeToLLVM(codegen, slice_type->slice.type);
      ptr = LLVMBuildLoad2(builder, LLVMPointerType(type, 0), ptr, "");
    } else {
      // Pointer (no length)
      type = typeToLLVM(codegen, slice_type->slice.type);
    }

    indices[0] = LLVMConstInt(
        LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size), 0, false);

    bool is_range = node->index.index->kind == NodeKind::Range;
    LLVMValueRef start = nullptr;
    LLVMValueRef end = nullptr;
    if (is_range) {
      start = gen(codegen, builder, node->index.index->range.min, scope);
      end = gen(codegen, builder, node->index.index->range.max, scope);
    } else {
      start = gen(codegen, builder, node->index.index, scope);
    }

    // Runtime length check
    if (length != nullptr) {
      LLVMValueRef func =
          codegen->function_stack[codegen->function_stack_len - 1].def;
      LLVMBasicBlockRef fail = LLVMAppendBasicBlockInContext(
          codegen->ctx, func, "bounds_check_fail");
      LLVMBasicBlockRef success = LLVMAppendBasicBlockInContext(
          codegen->ctx, func, "bounds_check_success");

      LLVMValueRef in_bounds =
          LLVMBuildICmp(builder, LLVMIntUGT, length, start, "");
      if (is_range) {
        LLVMIntPredicate op = LLVMIntUGT;
        if (node->index.index->range.mode == NodeRange::LessThan) {
          op = LLVMIntUGE;
        }

        LLVMValueRef end_in_bounds =
            LLVMBuildICmp(builder, op, length, end, "");
        in_bounds = LLVMBuildAnd(builder, in_bounds, end_in_bounds, "");
      }

      LLVMBuildCondBr(builder, in_bounds, success, fail);

      // Generate Fail
      LLVMPositionBuilderAtEnd(builder, fail);

      // TODO: Panic
      // FIXME: As of writing there is no way to handle a panic
      LLVMBuildUnreachable(builder); // this doesn't crash

      // Generate Success
      LLVMPositionBuilderAtEnd(builder, success);
    }

    // Index
    indices[0] = start;
    LLVMValueRef elem_ptr = LLVMBuildGEP2(builder, type, ptr, indices, 1, "");
    if (!is_range) {
      return elem_ptr;
    }

    // Get new slice length
    LLVMValueRef new_length = LLVMBuildSub(builder, end, start, "");
    LLVMTypeRef ptr_ty =
        LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size);
    if (node->index.index->range.mode == NodeRange::EqualTo) {
      new_length =
          LLVMBuildAdd(builder, new_length, LLVMConstInt(ptr_ty, 1, false), "");
    }

    // Create Slice
    LLVMValueRef constants[2];
    constants[0] = LLVMConstNull(LLVMTypeOf(elem_ptr));
    constants[1] = LLVMConstInt(LLVMTypeOf(new_length), 0, false);

    LLVMValueRef new_slice =
        LLVMConstStructInContext(codegen->ctx, constants, 2, false);
    new_slice = LLVMBuildInsertValue(builder, new_slice, elem_ptr, 0, "");
    new_slice = LLVMBuildInsertValue(builder, new_slice, new_length, 1, "");

    LLVMValueRef out_slice =
        LLVMBuildAlloca(builder, LLVMTypeOf(new_slice), "");
    LLVMBuildStore(builder, new_slice, out_slice);
    return out_slice;
  }
  case NodeKind::Import: {
    return addr(codegen, builder, node->import.node, node->import.scope);
  }
  }

  return nullptr;
}

void genFunctionBody(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                     Symbol *scope, LLVMTypeRef fn_type, LLVMValueRef func) {
  if (!node->function.undefined &&
      node->location.file.compare(codegen->source_path)) {
    LLVMBasicBlockRef entry =
        LLVMAppendBasicBlockInContext(codegen->ctx, func, "entry");
    LLVMBuilderRef body_builder = LLVMCreateBuilderInContext(codegen->ctx);
    LLVMPositionBuilderAtEnd(body_builder, entry);

    // Prepare Arguments
    size_t param_idx = 0;

    FnABICache *abi_cache = codegen->fn_abi_cache.get(fn_type);

    // Return as argument
    LLVMValueRef return_ptr = nullptr;
    LLVMTypeRef return_ty =
        typeToLLVM(codegen, node->function.return_type->value.data.type_value);

    if (abi_cache->return_arg.kind == ABIArgKind::Indirect) {
      return_ptr = LLVMGetParam(func, 0);
      if (abi_cache->return_arg.attribute != nullptr) {
        LLVMAddAttributeAtIndex(func, param_idx,
                                abi_cache->return_arg.attribute);
      }

      param_idx += 1;
    }

    // Prepare Parameters
    for (size_t i = 0; i < node->function.parameters.length; i++) {
      Node *key = node->function.parameters.data.ptr[i];
      char *name = (char *)codegen->allocator->alloc(key->field.name.len + 1);
      memcpy(name, key->field.name.ptr, key->field.name.len);
      name[key->field.name.len] = 0;

      LLVMTypeRef param_ty = typeToLLVM(codegen, key->value.type);
      LLVMValueRef alloca = LLVMBuildAlloca(body_builder, param_ty, name);
      ABIArg abi_arg = abi_cache->args.ptr[i];
      codegen->node_to_value.insert(key, alloca);

      if (abi_arg.kind == ABIArgKind::Ignore) {
        continue;
      } else if (abi_arg.kind == ABIArgKind::Direct) {
        LLVMBuildStore(body_builder, LLVMGetParam(func, param_idx), alloca);
      } else if (abi_arg.kind == ABIArgKind::Indirect) {
        // Dereference
        LLVMValueRef val = LLVMGetParam(func, param_idx);
        LLVMTypeRef ptr_ty = LLVMPointerType(abi_arg.type, 0);
        val = LLVMBuildLoad2(body_builder, ptr_ty, val, "");
        LLVMBuildStore(body_builder, val, alloca);
      }

      if (abi_arg.attribute != nullptr) {
        LLVMAddAttributeAtIndex(func, param_idx, abi_arg.attribute);
      }

      param_idx += 1;
    }

    // Generate body
    codegen->function_stack[codegen->function_stack_len] = {
        .def = func, .ret_ptr = return_ptr};
    codegen->function_defer_boundary[codegen->function_stack_len] =
        codegen->defer_stack_len;
    codegen->function_stack_len += 1;

    Symbol *fn_scope = scope->findSymbolByNode(node);
    gen(codegen, body_builder, node->function.body, fn_scope);
    codegen->function_stack_len -= 1;

    LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(body_builder);
    if (LLVMGetBasicBlockTerminator(insert_block) == nullptr) {
      LLVMBuildRetVoid(body_builder);
    }

    LLVMDisposeBuilder(body_builder);
  }
}

LLVMValueRef gen(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                 Symbol *scope) {
  switch (node->kind) {
  case NodeKind::Compound: {
    Symbol *compound_scope = scope->findSymbolByNode(node);
    if (compound_scope == nullptr) {
      compound_scope = scope;
    }

    size_t old_defer_len = codegen->defer_stack_len;

    for (size_t i = 0; i < node->children.length; i++) {
      gen(codegen, builder, node->children.data.ptr[i], compound_scope);
    }

    codegen->defer_stack_len = old_defer_len;
    break;
  }
  case NodeKind::Name: {
    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    LLVMValueRef *value = codegen->node_to_value.get(symbol->node);
    if (value == nullptr) {
      gen(codegen, builder, symbol->node, symbol->parent);
      value = codegen->node_to_value.get(symbol->node);
    }

    if (value == nullptr) {
      std::cerr << node->location << " Couldn't find value `" << node->text
                << "`. Aborting...\n";
      std::abort();
    }

    // TODO: A variable may not be generated for runtime
    return LLVMBuildLoad2(
        builder, typeToLLVM(codegen, symbol->node->value.type), *value, "");
  }
  case NodeKind::Bool: {
    return valueToLLVM(codegen, &node->value);
  }
  case NodeKind::Integer: {
    return valueToLLVM(codegen, &node->value);
  }
  case NodeKind::Float: {
    return valueToLLVM(codegen, &node->value);
  }
  case NodeKind::Char: {
    return valueToLLVM(codegen, &node->value);
  }
  case NodeKind::String: {
    return valueToLLVM(codegen, &node->value);
  }
  case NodeKind::Field: {
    LLVMValueRef *cache = codegen->node_to_value.get(node);
    if (cache != nullptr) {
      return nullptr;
    }

    // Get Symbol
    Symbol *field_symbol = scope->findSymbolByNode(node);

    // Get Name
    String name = {.ptr = nullptr};
    if (node->field.attributes != nullptr) {
      Node *link_name_node = nullptr;
      for (size_t i = 0; i < node->field.attributes->children.length; i++) {
        Node *attr = node->field.attributes->children.data.ptr[i];
        if (!attr->member.name.compare("link_name")) {
          continue;
        }

        link_name_node = attr->member.value;
        break;
      }

      if (link_name_node != nullptr &&
          link_name_node->kind == NodeKind::String) {
        name = link_name_node->text;
      }
    }

    if (name.ptr == nullptr) {
      name = field_symbol->mangleName(codegen->allocator);
    }

    // Generate Value
    if (node->value.type->kind == TypeKind::Function) {
      // Build function and set name
      LLVMTypeRef type = typeToLLVM(codegen, node->field.initial->value.type);
      LLVMValueRef func = LLVMAddFunction(codegen->mod, "", type);

      LLVMSetValueName2(func, (const char *)name.ptr, name.len);
      codegen->node_to_value.insert(node, func);

      genFunctionBody(codegen, builder, node->field.initial, field_symbol, type,
                      func);
    } else if (node->value.type->kind == TypeKind::TypeId) {
      // Type
      Type *real_type = node->value.data.type_value;
      if (real_type->kind == TypeKind::Struct) {
        char *c_name = (char *)malloc(sizeof(char) * name.len + 1);
        memcpy(c_name, (const char *)name.ptr, name.len);
        c_name[name.len] = 0;

        LLVMTypeRef type = typeToLLVM(codegen, real_type, c_name);
        free(c_name);
      }

      // Let the type generate it's children
      gen(codegen, builder, node->field.initial, field_symbol);
    } else if (scope->location_aware) {
      // Local Variable
      LLVMTypeRef type = typeToLLVM(codegen, node->value.type);
      LLVMValueRef alloca = LLVMBuildAlloca(builder, type, "");
      LLVMSetValueName2(alloca, (const char *)name.ptr, name.len);
      codegen->node_to_value.insert(node, alloca);

      LLVMValueRef value = LLVMConstNull(type);
      if (node->value.has_data) {
        value = valueToLLVM(codegen, &node->value);
      } else if (!node->field.undefined && node->field.initial != nullptr) {
        value = gen(codegen, builder, node->field.initial, field_symbol);
      }

      LLVMBuildStore(builder, value, alloca);
    } else {
      // Global Variable
      LLVMTypeRef type = typeToLLVM(codegen, node->value.type);
      LLVMValueRef alloca = LLVMAddGlobal(codegen->mod, type, "");
      LLVMSetValueName2(alloca, (const char *)name.ptr, name.len);
      codegen->node_to_value.insert(node, alloca);

      if (!node->field.undefined &&
          node->location.file.compare(codegen->source_path)) {
        LLVMSetInitializer(alloca, valueToLLVM(codegen, &node->value));
      }
    }
    break;
  }
  case NodeKind::Function: {
    LLVMTypeRef type = typeToLLVM(codegen, node->value.type);
    LLVMValueRef func = LLVMAddFunction(codegen->mod, "", type);
    genFunctionBody(codegen, builder, node, scope, type, func);
    return func;
  }
  case NodeKind::Struct: {
    Symbol *struct_scope = scope->findSymbolByNode(node);
    for (size_t i = 0; i < node->_struct.body.length; i++) {
      gen(codegen, builder, node->_struct.body.data.ptr[i], struct_scope);
    }
    break;
  }
  case NodeKind::Enum: {
    Symbol *enum_scope = scope->findSymbolByNode(node);
    for (size_t i = 0; i < node->_enum.body.length; i++) {
      gen(codegen, builder, node->_enum.body.data.ptr[i], enum_scope);
    }
    break;
  }
  case NodeKind::Union: {
    Symbol *union_scope = scope->findSymbolByNode(node);
    for (size_t i = 0; i < node->_union.body.length; i++) {
      gen(codegen, builder, node->_union.body.data.ptr[i], union_scope);
    }
    break;
  }
  case NodeKind::Namespace: {
    Symbol *namespace_scope = scope->findSymbolByNode(node);
    for (size_t i = 0; i < node->children.length; i++) {
      gen(codegen, builder, node->children.data.ptr[i], namespace_scope);
    }
    break;
  }
  case NodeKind::Member: {
    return valueToLLVM(codegen, &node->value);
  }
  case NodeKind::Import: {
    return gen(codegen, builder, node->import.node, node->import.scope);
  }
  case NodeKind::Const:
  case NodeKind::Slice: {
    // BLANK
    break;
  }
  case NodeKind::UnaryOperator: {
    return genUnary(codegen, builder, node, scope);
  }
  case NodeKind::Operator: {
    return genBinary(codegen, builder, node, scope);
  }
  case NodeKind::Call: {
    Type *callee_type = node->call.callee->value.type;
    LLVMTypeRef llvm_callee_type = typeToLLVM(codegen, callee_type);
    bool needs_dereference = false;
    if (callee_type->kind == TypeKind::Pointer) {
      callee_type = callee_type->child;
      needs_dereference = true;
    }

    // Get receiver
    LLVMValueRef receiver = nullptr;
    size_t has_receiver = 0;
    if (node->call.callee->kind == NodeKind::Operator &&
        node->call.callee->_operator.opcode == Operator::MemberAccess &&
        node->call.callee->_operator.lhs->value.type->kind !=
            TypeKind::TypeId) {
      receiver =
          addr(codegen, builder, node->call.callee->_operator.lhs, scope);
      has_receiver = 1;

      // Load
      Symbol *func_symbol = node->call.callee->value.type->function.scope;
      Node *arg0 = func_symbol->node->function.parameters.data.ptr[0];
      if (arg0->value.type->kind != TypeKind::Pointer) {
        receiver = LLVMBuildLoad2(
            builder, typeToLLVM(codegen, arg0->value.type), receiver, "");
      }
    }

    // Arguments
    ArrayList<LLVMValueRef> args;
    args.init(codegen->allocator, callee_type->function.arguments.length);

    FnABICache *abi_cache = codegen->fn_abi_cache.get(llvm_callee_type);

    // Return as argument
    LLVMTypeRef ret_ty = typeToLLVM(codegen, callee_type->function.return_type);
    LLVMValueRef ret_as_arg = nullptr;
    if (abi_cache->return_arg.kind == ABIArgKind::Indirect) {
      // Allocate return
      ret_as_arg =
          LLVMBuildAlloca(builder, abi_cache->return_arg.type, "return");
      args.push(ret_as_arg);
    }

    // Receiver argument
    if (receiver != nullptr) {
      args.push(receiver);
    }

    for (size_t i = 0; i < node->call.arguments.length; i++) {
      Node *arg = node->call.arguments.data.ptr[i];
      LLVMTypeRef ty = typeToLLVM(codegen, arg->value.type);
      ABIArg abi_arg = abi_cache->args.ptr[i];
      if (abi_arg.kind == ABIArgKind::Ignore) {
        continue;
      }

      // Messy argument casting
      if (abi_arg.kind == ABIArgKind::Indirect) {
        LLVMValueRef val = addr(codegen, builder, arg, scope);
        val = LLVMBuildBitCast(builder, val, abi_arg.type, "");
        args.push(val);
        continue;
      }

      LLVMValueRef alloca = LLVMBuildAlloca(builder, abi_arg.type, "");
      LLVMBuildStore(builder, gen(codegen, builder, arg, scope), alloca);
      args.push(LLVMBuildLoad2(builder, abi_arg.type, alloca, ""));
    }

    // Build Call
    LLVMValueRef function = addr(codegen, builder, node->call.callee, scope);
    if (needs_dereference) {
      function = LLVMBuildLoad2(
          builder, LLVMPointerType(typeToLLVM(codegen, callee_type), 0),
          function, "");
    }

    // Handle return
    LLVMValueRef ret = LLVMBuildCall2(builder, typeToLLVM(codegen, callee_type),
                                      function, args.data.ptr, args.length, "");

    if (abi_cache->return_arg.kind == ABIArgKind::Ignore) {
      return nullptr;
    }

    // Messy return casting
    if (ret_as_arg != nullptr) {
      ret = LLVMBuildBitCast(builder, ret_as_arg, ret_ty, "");
    } else {
      LLVMValueRef ret_alloca =
          LLVMBuildAlloca(builder, abi_cache->return_arg.type, "");
      LLVMBuildStore(builder, ret, ret_alloca);
      ret = ret_alloca;
    }
    return LLVMBuildLoad2(builder, ret_ty, ret, "");
  }
  case NodeKind::Index: {
    LLVMValueRef ptr = addr(codegen, builder, node, scope);
    return LLVMBuildLoad2(builder, typeToLLVM(codegen, node->value.type), ptr,
                          "");
  }
  case NodeKind::Initializer: {
    Node *record = node->initializer.record;
    LLVMTypeRef ty = typeToLLVM(codegen, node->value.type);
    LLVMValueRef agg = LLVMConstNull(ty);

    for (size_t i = 0; i < node->initializer.setters.length; i++) {
      Node *setter = node->initializer.setters.data.ptr[i];
      LLVMValueRef value = nullptr;
      size_t idx = i;

      if (node->initializer.is_list) {
        value = gen(codegen, builder, setter, scope);
      } else {
        Symbol *struct_symbol = record->value.data.type_value->_struct.scope;
        Node *struct_node = struct_symbol->node;

        for (size_t l = 0; l < struct_node->_struct.fields.length; l++) {
          Node *field = struct_node->_struct.fields.data.ptr[l];
          if (field->field.name.compare(setter->member.name)) {
            idx = l;
            value = gen(codegen, builder, setter->member.value, scope);
            break;
          }
        }
      }

      agg = LLVMBuildInsertValue(builder, agg, value, idx, "");
    }

    return agg;
  }
  case NodeKind::Return: {
    size_t defer_boundary =
        codegen->function_defer_boundary[codegen->function_stack_len];
    if (codegen->defer_stack_len - defer_boundary > 0) {
      size_t i = codegen->defer_stack_len;
      while (i > defer_boundary) {
        i -= 1;
        gen(codegen, builder, codegen->defer_stack[i], scope);
      }
    }

    if (node->child == nullptr) {
      LLVMBuildRetVoid(builder);
    } else {
      LLVMValueRef value = gen(codegen, builder, node->child, scope);

      FuncStackNode parent_func =
          codegen->function_stack[codegen->function_stack_len - 1];
      if (parent_func.ret_ptr != nullptr) {
        LLVMBuildStore(builder, value, parent_func.ret_ptr);
        LLVMBuildRetVoid(builder);
      } else {
        LLVMBuildRet(builder, value);
      }
    }

    break;
  }
  case NodeKind::If: {
    Symbol *if_scope = scope->findSymbolByNode(node);
    LLVMValueRef parent_function =
        codegen->function_stack[codegen->function_stack_len - 1].def;

    // Blocks
    LLVMBasicBlockRef then_block =
        LLVMAppendBasicBlockInContext(codegen->ctx, parent_function, "if_then");
    LLVMBasicBlockRef else_block = nullptr;
    if (node->_if._else != nullptr) {
      else_block = LLVMAppendBasicBlockInContext(codegen->ctx, parent_function,
                                                 "else_body");
    }

    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(
        codegen->ctx, parent_function, "if_merge");

    // Conditional
    LLVMValueRef condition =
        gen(codegen, builder, node->_if.conditional, if_scope);

    if (else_block != nullptr) {
      LLVMBuildCondBr(builder, condition, then_block, else_block);
    } else {
      LLVMBuildCondBr(builder, condition, then_block, merge_block);
    }

    // Body
    LLVMPositionBuilderAtEnd(builder, then_block);
    gen(codegen, builder, node->_if.body, if_scope);

    LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(builder);
    if (LLVMGetBasicBlockTerminator(insert_block) == nullptr) {
      LLVMBuildBr(builder, merge_block);
    }

    // Else
    if (else_block != nullptr) {
      LLVMPositionBuilderAtEnd(builder, else_block);

      Symbol *else_scope = scope->findSymbolByNode(node->_if._else);
      if (else_scope == nullptr) {
        else_scope = scope;
      }
      gen(codegen, builder, node->_if._else, scope);

      insert_block = LLVMGetInsertBlock(builder);
      if (LLVMGetBasicBlockTerminator(insert_block) == nullptr) {
        LLVMBuildBr(builder, insert_block);
      }
    }

    // Merge
    LLVMPositionBuilderAtEnd(builder, merge_block);
    break;
  }
  case NodeKind::For: {
    Symbol *for_scope = scope->findSymbolByNode(node);
    LLVMValueRef parent_function =
        codegen->function_stack[codegen->function_stack_len - 1].def;

    // Blocks
    LLVMBasicBlockRef condition_block = LLVMAppendBasicBlockInContext(
        codegen->ctx, parent_function, "for_condition");
    LLVMBasicBlockRef do_block =
        LLVMAppendBasicBlockInContext(codegen->ctx, parent_function, "for_do");
    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(
        codegen->ctx, parent_function, "for_merge");

    LLVMBuildBr(builder, condition_block);

    // Loop Stack
    codegen->loop_stack[codegen->loop_stack_len] = {
        .condition = condition_block, ._do = do_block, .merge = merge_block};
    codegen->loop_defer_boundary[codegen->loop_stack_len] =
        codegen->defer_stack_len;
    codegen->loop_stack_len += 1;

    // Conditional
    LLVMPositionBuilderAtEnd(builder, condition_block);
    LLVMValueRef condition =
        gen(codegen, builder, node->_for.conditional, for_scope);

    LLVMBuildCondBr(builder, condition, do_block, merge_block);

    // Do
    LLVMPositionBuilderAtEnd(builder, do_block);
    gen(codegen, builder, node->_for.body, for_scope);

    LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(builder);
    if (LLVMGetBasicBlockTerminator(insert_block) == nullptr) {
      LLVMBuildBr(builder, condition_block);
    }

    // Merge
    LLVMPositionBuilderAtEnd(builder, merge_block);
    codegen->loop_stack_len -= 1;
    break;
  }
  case NodeKind::Switch: {
    // Blocks
    LLVMBasicBlockRef merge_block =
        LLVMCreateBasicBlockInContext(codegen->ctx, "switch_merge");

    // Switch
    LLVMValueRef value =
        gen(codegen, builder, node->_switch.conditional, scope);
    LLVMValueRef _switch = LLVMBuildSwitch(builder, value, merge_block,
                                           node->_switch.cases.length);

    LLVMValueRef parent_function =
        codegen->function_stack[codegen->function_stack_len - 1].def;

    // Cases
    for (size_t i = 0; i < node->_switch.cases.length; i++) {
      Node *_case = node->_switch.cases.data.ptr[i];
      Symbol *case_scope = scope->findSymbolByNode(_case->_case.body);

      // Body
      LLVMBasicBlockRef case_block = LLVMAppendBasicBlockInContext(
          codegen->ctx, parent_function, "switch_case");
      LLVMPositionBuilderAtEnd(builder, case_block);
      gen(codegen, builder, _case->_case.body, case_scope);

      LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(builder);
      if (LLVMGetBasicBlockTerminator(insert_block) == nullptr) {
        LLVMBuildBr(builder, merge_block);
      }

      // Add
      LLVMValueRef constant =
          valueToLLVM(codegen, &_case->_case.constant->value);
      LLVMAddCase(_switch, constant, case_block);
    }

    // Merge
    LLVMAppendExistingBasicBlock(parent_function, merge_block);
    LLVMPositionBuilderAtEnd(builder, merge_block);
    break;
  }
  case NodeKind::Break:
  case NodeKind::Continue: {
    size_t defer_boundary =
        codegen->loop_defer_boundary[codegen->loop_stack_len];
    if (codegen->defer_stack_len - defer_boundary > 0) {
      size_t i = codegen->defer_stack_len;
      while (i > defer_boundary) {
        i -= 1;
        gen(codegen, builder, codegen->defer_stack[i], scope);
      }
    }

    // TODO: Named Loop
    LoopBlocks blocks = codegen->loop_stack[codegen->loop_stack_len - 1];
    LLVMBasicBlockRef block = blocks.condition;
    if (node->kind == NodeKind::Break) {
      block = blocks.merge;
    }
    LLVMBuildBr(builder, block);
    break;
  }
  case NodeKind::Defer: {
    codegen->defer_stack[codegen->defer_stack_len] = node->child;
    codegen->defer_stack_len += 1;
    break;
  }
  case NodeKind::Assembly: {
    genAssembly(codegen, builder, node, scope);
    break;
  }
  }

  return nullptr;
}

void CodeGenModule::generate(CodeGenContext *context, bool emit_ir,
                             bool emit_asm, Optimization opt) {
  // Setup State
  char *name =
      (char *)allocator->alloc(sizeof(char) * this->source_path.len + 1);
  memcpy(name, this->source_path.ptr, this->source_path.len);
  *(name + this->source_path.len) = 0;

  this->type_to_llvm.init(this->allocator, 32);
  this->node_to_value.init(this->allocator, 32);
  this->fn_abi_cache.init(this->allocator, 32);
  this->defer_stack_len = 0;
  this->loop_stack_len = 0;
  this->function_stack_len = 0;

  // Setup Module
  this->ctx = context->ctx;
  this->mod = LLVMModuleCreateWithNameInContext(name, this->ctx);

  // Setup target info
  LLVMSetTarget(this->mod, context->target_triple);
  LLVMSetDataLayout(this->mod, context->data_layout_str);

  // Get Pointer size
  LLVMTypeRef tmp_ptr = LLVMPointerType(LLVMInt1TypeInContext(this->ctx), 0);
  this->pointer_size =
      LLVMSizeOfTypeInBits(LLVMGetModuleDataLayout(this->mod), tmp_ptr);
  this->target_abi = ABIcreateTarget(context->abi);

  // Generate
  gen(this, nullptr, this->ast, this->symbol);

  // Optimize
  if (opt != Optimization::None) {
    const char *passes = "sroa,simplifycfg,instcombine,gvn,licm,dce,"
                         "indvars,loop-unroll,tailcallelim,early-cse";
    LLVMPassBuilderOptionsRef pass_options = LLVMCreatePassBuilderOptions();
    LLVMRunPasses(this->mod, passes, context->target_machine, pass_options);
    LLVMDisposePassBuilderOptions(pass_options);
  }

  // Cleanup
  char *output_path =
      (char *)allocator->alloc(sizeof(char) * this->output_path.len + 1);
  memcpy(output_path, this->output_path.ptr, this->output_path.len);
  *(output_path + this->output_path.len) = 0;
  char *error = nullptr;
  LLVMBool fail = 0;

  if (emit_ir) {
    fail = LLVMPrintModuleToFile(this->mod, output_path, &error);
  } else {
    LLVMCodeGenFileType file_type =
        emit_asm ? LLVMAssemblyFile : LLVMObjectFile;
    fail = LLVMTargetMachineEmitToFile(context->target_machine, this->mod,
                                       output_path, file_type, &error);
  }

  // Handle Fail
  if (fail) {
    std::cerr << "LLVM Error: " << error << "\n";
    std::cerr << "Failed to write llvm ir bitcode to file. Aborting.\n";
    LLVMDisposeMessage(error);
    std::abort();
  }

  // Cleanup
  LLVMDisposeMessage(error);

  this->type_to_llvm.deinit();
  this->node_to_value.deinit();
  this->fn_abi_cache.deinit();
}

void CodeGenContext::init() {
  // Initialize
  LLVMInitializeAllTargetInfos();
  LLVMInitializeAllTargets();
  LLVMInitializeAllTargetMCs();
  LLVMInitializeAllAsmParsers();
  LLVMInitializeAllAsmPrinters();

  // Target Info
  this->target_triple = LLVMGetDefaultTargetTriple();
  LLVMTargetRef target = nullptr;
  char *errors;
  if (LLVMGetTargetFromTriple(this->target_triple, &target, &errors)) {
    std::cerr << "Error getting target for codegen\n";
    std::cerr << errors << "\n";
    return;
  }

  this->target_machine = LLVMCreateTargetMachine(
      target, target_triple, "", "", LLVMCodeGenLevelDefault, LLVMRelocDefault,
      LLVMCodeModelDefault);
  this->target_data = LLVMCreateTargetDataLayout(target_machine);
  this->data_layout_str = LLVMCopyStringRepOfTargetData(target_data);

  // Context
  this->ctx = LLVMContextCreate();
  this->abi = ABI::SystemV_Amd64;
}

void CodeGenContext::deinit() {
  LLVMDisposeTargetData(this->target_data);
  LLVMDisposeTargetMachine(this->target_machine);
  LLVMDisposeMessage(this->data_layout_str);
}
