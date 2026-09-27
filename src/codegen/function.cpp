#include "abi/general.hpp"
#include "codegen.hpp"
#include "define.hpp"
#include "llvm-c/Types.h"
#include <cstring>
#include <llvm-c/Core.h>

void genFunctionBody(CodeGenModule *codegen, LLVMBuilderRef builder,
                     RIRValue *inst) {
  if (inst->function.undefined) {
    return;
  }

  LLVMValueRef func = *codegen->inst_to_llvm.get(inst->id);
  codegen->parent_function = func;
  codegen->parent_function_type =
      *codegen->type_to_llvm.get(inst->function.type);
  codegen->return_arg = nullptr;
  codegen->function_arg_index = 0;

  // Blocks
  codegen->define_block =
      LLVMAppendBasicBlockInContext(codegen->ctx, func, "defines");

  for (size_t i = 0; i < inst->function.blocks.length; i++) {
    RIRBlockId block_id = inst->function.blocks.getUnchecked(i);

    // char *name = (char *)codegen->allocator->allocZeroed(block->name.len +
    // 1); memcpy(name, block->name.ptr, block->name.len);
    LLVMBasicBlockRef llvm_block =
        LLVMAppendBasicBlockInContext(codegen->ctx, func, "");
    codegen->block_to_llvm.insert(block_id, llvm_block);
  }

  // ABI Return
  FnABICache *abi = codegen->fn_abi_cache.get(codegen->parent_function_type);
  if (abi->return_arg.kind == ABIArgKind::Indirect) {
    codegen->return_arg = LLVMGetParam(func, 0);
    codegen->function_arg_index += 1;
  }

  // Code
  for (size_t i = 0; i < inst->function.blocks.length; i++) {
    RIRBlockId block_id = inst->function.blocks.getUnchecked(i);
    LLVMBasicBlockRef llvm_block = *codegen->block_to_llvm.get(block_id);
    LLVMPositionBuilderAtEnd(builder, llvm_block);

    RIRBlock *block = codegen->rir_context->getBlock(block_id);
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
                     RIRValue *inst) {
  RIRValue *callee = codegen->rir_context->getInst(inst->call.callee);
  RIRTypeId callee_type_id = callee->result;
  RIRType *callee_type = codegen->rir_context->getType(callee_type_id);
  LLVMTypeRef llvm_callee_type = typeToLLVM(codegen, callee_type_id);

  // Arguments
  ArrayList<LLVMValueRef> args;
  args.init(codegen->allocator, callee_type->function.arguments.len);

  FnABICache *abi_cache = codegen->fn_abi_cache.get(llvm_callee_type);

  // Return as argument
  LLVMTypeRef ret_ty = typeToLLVM(codegen, callee_type->function._return);
  LLVMValueRef ret_as_arg = nullptr;
  if (abi_cache->return_arg.kind == ABIArgKind::Indirect) {
    // Allocate return
    ret_as_arg =
        BuildAlloca(codegen, builder, abi_cache->return_arg.type, "return");
    args.push(ret_as_arg);
  }

  for (size_t i = 0; i < callee_type->function.arguments.len; i++) {
    RIRTypeId arg_type_id = callee_type->function.arguments.ptr[i];
    LLVMTypeRef ty = typeToLLVM(codegen, arg_type_id);
    ABIArg abi_arg = abi_cache->args.ptr[i];
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
  LLVMValueRef ret =
      LLVMBuildCall2(builder, typeToLLVM(codegen, callee_type_id), function,
                     args.data.ptr, args.length, "");

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
