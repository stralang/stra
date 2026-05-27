#include "codegen.hpp"
#include "ast.hpp"
#include "containers.hpp"
#include "operator.hpp"
#include "print.hpp"
#include "symbol.hpp"
#include "types.hpp"
#include "llvm-c/BitWriter.h"
#include "llvm-c/Core.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
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

LLVMTypeRef typeToLLVM(CodeGenModule *codegen, Type *type) {
  switch (type->kind) {
  case TypeKind::Void: {
    return LLVMVoidTypeInContext(codegen->ctx);
  }
  case TypeKind::Bool: {
    return LLVMInt1TypeInContext(codegen->ctx);
  }
  case TypeKind::Integer: {
    size_t bits = type->integer.bits;
    if (type->integer.bits == -1) {
      LLVMTypeRef tmp_type =
          LLVMPointerType(LLVMInt1TypeInContext(codegen->ctx), 0);
      bits =
          LLVMSizeOfTypeInBits(LLVMGetModuleDataLayout(codegen->mod), tmp_type);
    }
    return LLVMIntTypeInContext(codegen->ctx, bits);
  }
  case TypeKind::Float: {
    switch (type->_float.bits) {
    case 16: {
      return LLVMHalfTypeInContext(codegen->ctx);
    }
    case 32: {
      return LLVMFloatTypeInContext(codegen->ctx);
    }
    case 64: {
      return LLVMDoubleTypeInContext(codegen->ctx);
    }
    case 128: {
      return LLVMFP128TypeInContext(codegen->ctx);
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
      types[1] = LLVMIntTypeInContext(codegen->ctx, bits);
      return LLVMStructTypeInContext(codegen->ctx, types, 2, false);
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

    LLVMTypeRef ty = LLVMStructTypeInContext(
        codegen->ctx, field_types, type->_struct.fields.length, false);
    codegen->scope_to_type.insert(type->_struct.scope, ty);
    return ty;
  }
  case TypeKind::Enum: {
    return typeToLLVM(codegen, type->_enum.repr_type);
  }
  case TypeKind::Union: {
    LLVMTypeRef tmp_ptr =
        LLVMPointerType(LLVMInt1TypeInContext(codegen->ctx), 0);
    size_t native_size =
        LLVMSizeOfTypeInBits(LLVMGetModuleDataLayout(codegen->mod), tmp_ptr);

    size_t data_size =
        type->sizeBits(native_size) - type->_union.repr_type->integer.bits;
    LLVMTypeRef tys[2];
    tys[0] = LLVMIntTypeInContext(codegen->ctx,
                                  type->_union.repr_type->integer.bits);
    tys[1] = LLVMArrayType(LLVMInt8TypeInContext(codegen->ctx), data_size);
    return LLVMStructTypeInContext(codegen->ctx, tys, 2, false);
  }
  }

  return nullptr;
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
  } else if (lhs_type->kind == TypeKind::Enum) {
    Symbol *enum_symbol = lhs_type->_enum.scope;
    Node *enum_node = enum_symbol->node;

    // Child stuff
    impl_children = &enum_node->_enum.body;
    impl_scope = enum_symbol;

    // Find constant
    size_t member_idx = SIZE_MAX;
    for (size_t i = 0; i < enum_node->_enum.members.length; i++) {
      Node *member = enum_node->_enum.members.data.ptr[i];
      if (member->member.name.compare(node->_operator.rhs->text)) {
        member_idx = i;
        break;
      }
    }

    if (member_idx != SIZE_MAX) {
      int64_t value =
          enum_node->_enum.members.data.ptr[member_idx]->value.data.integer;
      return LLVMConstInt(typeToLLVM(codegen, enum_node->value.type), value,
                          enum_node->value.type->integer.is_signed);
    }
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
      if (variant->field.name.compare(node->text)) {
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
      indices[1] = LLVMConstInt(index_ty, 1, false);
      LLVMValueRef data = LLVMBuildGEP2(
          builder, typeToLLVM(codegen, lhs->value.type), value, indices, 2, "");

      // Cast
      LLVMTypeRef dest_ty = typeToLLVM(
          codegen,
          union_node->value.type->_union.variants.data.ptr[variant_idx]);
      return LLVMBuildBitCast(builder, data, dest_ty, "");
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
    } else if (real_ty->kind == TypeKind::Struct) {
      Symbol *union_symbol = real_ty->_union.scope;
      impl_children = &union_symbol->node->_union.body;
      impl_scope = union_symbol;
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
    LLVMBuildStore(builder, rhs_value, lhs_ptr);
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
    indices[0] = LLVMConstInt(LLVMInt1TypeInContext(codegen->ctx), 0, false);
    if (slice_type->slice.length > 0) {
      // Array (compile-time length)
      type = typeToLLVM(codegen, slice_type);

      LLVMTypeRef tmp_type =
          LLVMPointerType(LLVMInt1TypeInContext(codegen->ctx), 0);
      uint64_t native_int =
          LLVMSizeOfTypeInBits(LLVMGetModuleDataLayout(codegen->mod), tmp_type);

      length = LLVMConstInt(LLVMIntTypeInContext(codegen->ctx, native_int),
                            slice_type->slice.length, false);
    } else if (slice_type->slice.length == 0) {
      // Slice (runtime length)
      length = LLVMBuildExtractValue(builder, slice, 1, "");

      indices[1] = indices[0];

      ptr = LLVMBuildGEP2(builder, typeToLLVM(codegen, slice_type), slice,
                          indices, 2, "");
      type = LLVMPointerType(typeToLLVM(codegen, slice_type->slice.type), 0);

    } else {
      // Pointer (no length)
      type = LLVMPointerType(typeToLLVM(codegen, slice_type->slice.type), 0);
    }

    LLVMValueRef index = gen(codegen, builder, node->index.index, scope);

    // Runtime length check
    if (length != nullptr) {
      LLVMValueRef func =
          codegen->function_stack[codegen->function_stack_len - 1];
      LLVMBasicBlockRef fail = LLVMAppendBasicBlockInContext(
          codegen->ctx, func, "bounds_check_fail");
      LLVMBasicBlockRef success = LLVMAppendBasicBlockInContext(
          codegen->ctx, func, "bounds_check_success");

      LLVMValueRef in_bounds =
          LLVMBuildICmp(builder, LLVMIntUGT, length, index, "");
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
    indices[1] = index;
    return LLVMBuildGEP2(builder, type, ptr, indices, 2, "");
  }
  case NodeKind::Import: {
    return addr(codegen, builder, node->import.node, node->import.scope);
  }
  }

  return nullptr;
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
      LLVMValueRef func =
          gen(codegen, builder, node->field.initial, field_symbol);
      LLVMSetValueName2(func, (const char *)name.ptr, name.len);
      codegen->node_to_value.insert(node, func);
    } else if (node->value.type->kind == TypeKind::TypeId) {
      // Type
      Type *real_type = node->value.data.type_value;
      if (real_type->kind == TypeKind::Struct) {
        LLVMTypeRef type = typeToLLVM(codegen, real_type);
        // TODO: Set name
      }

      // Let the type generate it's children
      gen(codegen, builder, node->field.initial, field_symbol);
    } else if (scope->location_aware) {
      // Local Variable
      LLVMTypeRef type = typeToLLVM(codegen, node->value.type);
      LLVMValueRef alloca = LLVMBuildAlloca(builder, type, "");
      LLVMSetValueName2(alloca, (const char *)name.ptr, name.len);
      codegen->node_to_value.insert(node, alloca);

      if (node->value.has_data) {
        LLVMBuildStore(builder, valueToLLVM(codegen, &node->value), alloca);
      } else if (!node->field.undefined && node->field.initial != nullptr) {
        LLVMValueRef initial =
            gen(codegen, builder, node->field.initial, field_symbol);
        LLVMBuildStore(builder, initial, alloca);
      }
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

    if (!node->function.undefined &&
        node->location.file.compare(codegen->source_path)) {
      LLVMBasicBlockRef entry =
          LLVMAppendBasicBlockInContext(codegen->ctx, func, "entry");
      LLVMBuilderRef body_builder = LLVMCreateBuilderInContext(codegen->ctx);
      LLVMPositionBuilderAtEnd(body_builder, entry);

      // Prepare Parameters
      for (size_t i = 0; i < node->function.parameters.length; i++) {
        Node *key = node->function.parameters.data.ptr[i];
        char *name = (char *)codegen->allocator->alloc(key->field.name.len + 1);
        memcpy(name, key->field.name.ptr, key->field.name.len);
        name[key->field.name.len] = 0;

        LLVMValueRef alloca = LLVMBuildAlloca(
            body_builder, typeToLLVM(codegen, key->value.type), name);
        LLVMBuildStore(body_builder, LLVMGetParam(func, i), alloca);
        codegen->node_to_value.insert(key, alloca);
      }

      // Generate body
      codegen->function_stack[codegen->function_stack_len] = func;
      codegen->function_defer_boundary[codegen->function_stack_len] =
          codegen->defer_stack_len;
      codegen->function_stack_len += 1;

      Symbol *fn_scope = scope->findSymbolByNode(node);
      gen(codegen, body_builder, node->function.body, fn_scope);
      codegen->function_stack_len -= 1;

      if (LLVMGetBasicBlockTerminator(entry) == nullptr) {
        LLVMBuildRetVoid(body_builder);
      }

      LLVMDisposeBuilder(body_builder);
    }

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
    LLVMValueRef *args = (LLVMValueRef *)codegen->allocator->alloc(
        sizeof(LLVMValueRef) *
        (callee_type->function.arguments.length + has_receiver));
    if (receiver != nullptr) {
      args[0] = receiver;
    }

    for (size_t i = 0; i < node->call.arguments.length; i++) {
      Node *arg = node->call.arguments.data.ptr[i];
      args[i + has_receiver] = gen(codegen, builder, arg, scope);
    }

    LLVMValueRef function = addr(codegen, builder, node->call.callee, scope);
    LLVMValueRef ret =
        LLVMBuildCall2(builder, typeToLLVM(codegen, callee_type), function,
                       args, callee_type->function.arguments.length, "");

    return ret;
  }
  case NodeKind::Index: {
    LLVMValueRef ptr = addr(codegen, builder, node, scope);
    return LLVMBuildLoad2(builder, typeToLLVM(codegen, node->value.type), ptr,
                          "");
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
      LLVMBuildRet(builder, gen(codegen, builder, node->child, scope));
    }

    break;
  }
  case NodeKind::If: {
    Symbol *if_scope = scope->findSymbolByNode(node);
    LLVMValueRef parent_function =
        codegen->function_stack[codegen->function_stack_len - 1];

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
    LLVMBuildBr(builder, merge_block);

    // Else
    if (else_block != nullptr) {
      LLVMPositionBuilderAtEnd(builder, else_block);

      Symbol *else_scope = scope->findSymbolByNode(node->_if._else);
      if (else_scope == nullptr) {
        else_scope = scope;
      }
      gen(codegen, builder, node->_if._else, scope);

      if (LLVMGetBasicBlockTerminator(else_block) == nullptr) {
        LLVMBuildBr(builder, merge_block);
      }
    }

    // Merge
    LLVMPositionBuilderAtEnd(builder, merge_block);
    break;
  }
  case NodeKind::For: {
    Symbol *for_scope = scope->findSymbolByNode(node);
    LLVMValueRef parent_function =
        codegen->function_stack[codegen->function_stack_len - 1];

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

    if (LLVMGetBasicBlockTerminator(do_block) == nullptr) {
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
        codegen->function_stack[codegen->function_stack_len - 1];

    // Cases
    for (size_t i = 0; i < node->_switch.cases.length; i++) {
      Node *_case = node->_switch.cases.data.ptr[i];
      Symbol *case_scope = scope->findSymbolByNode(_case->_case.body);

      // Body
      LLVMBasicBlockRef case_block = LLVMAppendBasicBlockInContext(
          codegen->ctx, parent_function, "switch_case");
      LLVMPositionBuilderAtEnd(builder, case_block);
      gen(codegen, builder, _case->_case.body, case_scope);

      if (LLVMGetBasicBlockTerminator(case_block) == nullptr) {
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

void CodeGenModule::generate(CodeGenContext *context) {
  // Setup State
  char *name =
      (char *)allocator->alloc(sizeof(char) * this->source_path.len + 1);
  memcpy(name, this->source_path.ptr, this->source_path.len);
  *(name + this->source_path.len) = 0;

  this->scope_to_type.init(this->allocator, 32);
  this->node_to_value.init(this->allocator, 32);
  this->defer_stack_len = 0;
  this->loop_stack_len = 0;
  this->function_stack_len = 0;

  // Setup Module
  this->ctx = context->ctx;
  this->mod = LLVMModuleCreateWithNameInContext(name, this->ctx);

  // Setup target info
  LLVMSetTarget(this->mod, context->target_triple);
  LLVMSetDataLayout(this->mod, context->data_layout_str);

  // Generate
  gen(this, nullptr, this->ast, this->symbol);

  // Cleanup
  char *output_path =
      (char *)allocator->alloc(sizeof(char) * this->output_path.len + 1);
  memcpy(output_path, this->output_path.ptr, this->output_path.len);
  *(output_path + this->output_path.len) = 0;
  char *error = nullptr;
  if (LLVMPrintModuleToFile(this->mod, output_path, &error)) {
    std::cerr << "LLVM Error: " << error << "\n";
    std::cerr << "Failed to write llvm ir bitcode to file. Aborting.\n";
    std::abort();
  }

  this->scope_to_type.deinit();
  this->node_to_value.deinit();
}

void CodeGenContext::init() {
  // Initialize
  LLVMInitializeNativeTarget();

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
}

void CodeGenContext::deinit() {
  LLVMDisposeTargetData(this->target_data);
  LLVMDisposeTargetMachine(this->target_machine);
  LLVMDisposeMessage(this->data_layout_str);
}
