#include "../helper.hpp"
#include "../print.hpp"
#include "abi/general.hpp"
#include "codegen.hpp"
#include "define.hpp"
#include "mir.hpp"
#include "llvm-c/Types.h"
#include <cstring>
#include <llvm-c/Core.h>

void genFunctionBody(CodeGenModule *codegen, LLVMBuilderRef builder,
                     MIRValue *inst) {
  if (inst->function.undefined) {
    return;
  }

  LLVMValueRef func = *codegen->inst_to_llvm.get(inst);
  codegen->parent_function = func;
  codegen->parent_function_type = *codegen->type_to_llvm.get(inst->result_type);
  codegen->return_arg = nullptr;
  codegen->function_arg_index = 0;

  // Blocks
  codegen->define_block =
      LLVMAppendBasicBlockInContext(codegen->ctx, func, "defines");

  for (size_t i = 0; i < inst->function.blocks.length; i++) {
    MIRBlock *block = inst->function.blocks.getUnchecked(i);

    char *name = (char *)codegen->allocator->alloc(inst->name.len + 1);
    memcpy(name, inst->name.ptr, inst->name.len);
    LLVMBasicBlockRef llvm_block =
        LLVMAppendBasicBlockInContext(codegen->ctx, func, name);
    codegen->block_to_llvm.insert(block, llvm_block);
  }

  // ABI Return
  FnABICache *abi = codegen->fn_abi_cache.get(codegen->parent_function_type);
  if (abi->return_arg.kind == ABIArgKind::Indirect) {
    codegen->return_arg = LLVMGetParam(func, 0);
    codegen->function_arg_index += 1;
  }

  // Code
  for (size_t i = 0; i < inst->function.blocks.length; i++) {
    MIRBlock *block = inst->function.blocks.getUnchecked(i);
    LLVMBasicBlockRef llvm_block = *codegen->block_to_llvm.get(block);
    LLVMPositionBuilderAtEnd(builder, llvm_block);

    for (size_t l = 0; l < block->instructions.length; l++) {
      gen(codegen, builder, block->instructions.getUnchecked(l));
    }
  }

  // Finish define block
  LLVMPositionBuilderAtEnd(builder, codegen->define_block);
  LLVMBuildBr(builder, *codegen->block_to_llvm.get(
                           inst->function.blocks.getUnchecked(0)));
}

LLVMValueRef genCall(CodeGenModule *codegen, LLVMBuilderRef builder,
                     MIRValue *inst) {
  Type *callee_type = inst->call.callee->result_type;
  bool needs_dereference = false;
  if (callee_type->kind == TypeKind::Pointer) {
    callee_type = callee_type->child;
    needs_dereference = true;
  }

  LLVMTypeRef llvm_callee_type = typeToLLVM(codegen, callee_type);

  // Get receiver
  LLVMValueRef receiver = nullptr;
  size_t has_receiver = 0;
  if (inst->call.receiver != nullptr) {
    receiver = getReference(codegen, inst->call.receiver);
    has_receiver = 1;
  }

  // Arguments
  ArrayList<LLVMValueRef> args;
  args.init(codegen->allocator, callee_type->function.arguments.len);

  FnABICache *abi_cache = codegen->fn_abi_cache.get(llvm_callee_type);

  // Return as argument
  LLVMTypeRef ret_ty = typeToLLVM(codegen, callee_type->function.return_type);
  LLVMValueRef ret_as_arg = nullptr;
  if (abi_cache->return_arg.kind == ABIArgKind::Indirect) {
    // Allocate return
    ret_as_arg =
        BuildAlloca(codegen, builder, abi_cache->return_arg.type, "return");
    args.push(ret_as_arg);
  }

  // Receiver argument
  if (receiver != nullptr) {
    ABIArg *abi_arg = abi_cache->args.ptr + 0;
    LLVMTypeRef abi_ty = abi_arg->type;
    receiver = BuildABICast(builder, receiver, LLVMPointerType(abi_ty, 0));

    // Dereference
    if (abi_arg->kind == ABIArgKind::Direct &&
        callee_type->function.arguments.ptr[0]->kind != TypeKind::Pointer) {
      receiver = LLVMBuildLoad2(builder, abi_ty, receiver, "");
    }

    args.push(receiver);
  }

  for (size_t i = 0; i < callee_type->function.arguments.len; i++) {
    Type *arg_type = callee_type->function.arguments.ptr[i];
    LLVMTypeRef ty = typeToLLVM(codegen, arg_type);
    ABIArg abi_arg = abi_cache->args.ptr[i + has_receiver];
    if (abi_arg.kind == ABIArgKind::Ignore) {
      continue;
    }

    // Messy argument casting
    if (abi_arg.kind == ABIArgKind::Indirect) {
      LLVMValueRef val = getReference(codegen, inst->call.arguments.ptr[i]);
      val = BuildABICast(builder, val, LLVMPointerType(abi_arg.type, 0));
      args.push(val);
      continue;
    }

    LLVMValueRef val = getReference(codegen, inst->call.arguments.ptr[i]);
    val = BuildABICast(builder, val, abi_arg.type);
    args.push(val);
  }

  // Build Call
  LLVMValueRef function = getReference(codegen, inst->call.callee);
  if (needs_dereference) {
    function = LLVMBuildLoad2(builder, llvm_callee_type, function, "");
  }

  LLVMValueRef ret = LLVMBuildCall2(builder, typeToLLVM(codegen, callee_type),
                                    function, args.data.ptr, args.length, "");

  // Handle return
  if (abi_cache->return_arg.kind == ABIArgKind::Ignore) {
    return nullptr;
  }

  // Messy return casting
  if (ret_as_arg != nullptr) {
    ret = BuildABICast(builder, ret_as_arg, LLVMPointerType(ret_ty, 0));
    ret = LLVMBuildLoad2(builder, ret_ty, ret, "");
  } else {
    ret = BuildABICast(builder, ret, ret_ty);
  }
  return ret;
}
