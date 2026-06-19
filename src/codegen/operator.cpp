#include "../print.hpp"
#include "codegen.hpp"
#include "define.hpp"
#include <iostream>
#include <llvm-c/Core.h>

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

LLVMValueRef addrCastAs(CodeGenModule *codegen, LLVMBuilderRef builder,
                        Node *node, Symbol *scope) {
  Type *src_type = node->_operator.lhs->value.type;
  Type *dst_type = node->_operator.rhs->value.data.type_value;
  LLVMTypeRef dst_llvm_type = typeToLLVM(codegen, dst_type);

  // Reuse casts
  if (src_type->kind == TypeKind::Slice && dst_type->kind == TypeKind::Slice) {
    // Slice to slice cast

    if (src_type->slice.length == 0 && dst_type->slice.length == 0) {
      return gen(codegen, builder, node->_operator.lhs, scope);
    }

    if (src_type->slice.length > 0 && dst_type->slice.length == 0) {
      LLVMValueRef lhs_ptr = addr(codegen, builder, node->_operator.lhs, scope);

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
          LLVMBuildAlloca(builder, LLVMTypeOf(new_slice), "");
      LLVMBuildStore(builder, new_slice, out_slice);
      return out_slice;
    }
  }

  std::cerr << "Unhandled address `as` cast in codegen\n";
  std::cerr << "Src `" << *src_type << "`\nDst `" << *dst_type << "`\n";
  std::abort();
}

LLVMValueRef genAssignment(CodeGenModule *codegen, LLVMBuilderRef builder,
                           Node *node, Symbol *scope) {
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

      LLVMTypeRef repr_type = typeToLLVM(codegen, union_type->_union.repr_type);
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

LLVMValueRef genCastAs(CodeGenModule *codegen, LLVMBuilderRef builder,
                       Node *node, Symbol *scope) {
  Type *src_type = node->_operator.lhs->value.type;
  Type *dst_type = node->_operator.rhs->value.data.type_value;
  LLVMTypeRef dst_llvm_type = typeToLLVM(codegen, dst_type);

  // Reuse casts
  if (src_type->kind == TypeKind::Slice && dst_type->kind == TypeKind::Slice) {
    LLVMValueRef ptr = addrCastAs(codegen, builder, node, scope);
    return LLVMBuildLoad2(builder, dst_llvm_type, ptr, "");
  }

  // Value casts
  if (src_type->kind == TypeKind::SIMD) {
    src_type = src_type->child;
  }

  LLVMValueRef lhs_value = gen(codegen, builder, node->_operator.lhs, scope);
  if (src_type->kind == TypeKind::Integer) {
    // Integer Cast
    if (dst_type->kind == TypeKind::Float && src_type->integer.is_signed) {
      return LLVMBuildSIToFP(builder, lhs_value, dst_llvm_type, "");
    } else if (dst_type->kind == TypeKind::Float &&
               !src_type->integer.is_signed) {
      return LLVMBuildUIToFP(builder, lhs_value, dst_llvm_type, "");
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
    return LLVMBuildPointerCast(builder, lhs_value, dst_llvm_type, "");
  }

  std::cerr << "Unhandled `as` cast in codegen\n";
  std::cerr << "Src `" << *src_type << "`\nDst `" << *dst_type << "`\n";
  std::abort();
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

  // Cast
  if (node->_operator.opcode == Operator::As) {
    return genCastAs(codegen, builder, node, scope);
  } else if (node->_operator.opcode == Operator::Bitcast) {
    LLVMValueRef lhs_value = gen(codegen, builder, node->_operator.lhs, scope);
    LLVMTypeRef dest_ty =
        typeToLLVM(codegen, node->_operator.rhs->value.data.type_value);
    return LLVMBuildBitCast(builder, lhs_value, dest_ty, "");
  }

  if (lhs_type->kind == TypeKind::SIMD) {
    lhs_type = lhs_type->child;
  }

  LLVMValueRef lhs_value = gen(codegen, builder, node->_operator.lhs, scope);
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
