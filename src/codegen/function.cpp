#include "codegen.hpp"
#include "define.hpp"
#include <llvm-c/Core.h>

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

LLVMValueRef genCall(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                     Symbol *scope) {
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
      node->call.callee->_operator.lhs->value.type->kind != TypeKind::TypeId) {
    receiver = addr(codegen, builder, node->call.callee->_operator.lhs, scope);
    has_receiver = 1;

    // Load
    Symbol *func_symbol = node->call.callee->value.type->function.scope;
    Node *arg0 = func_symbol->node->function.parameters.data.ptr[0];
    if (arg0->value.type->kind != TypeKind::Pointer) {
      receiver = LLVMBuildLoad2(builder, typeToLLVM(codegen, arg0->value.type),
                                receiver, "");
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
    ret_as_arg = LLVMBuildAlloca(builder, abi_cache->return_arg.type, "return");
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
        builder, LLVMPointerType(typeToLLVM(codegen, callee_type), 0), function,
        "");
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
  return ret;
}
