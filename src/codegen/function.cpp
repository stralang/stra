#include "../helper.hpp"
#include "../print.hpp"
#include "abi/general.hpp"
#include "codegen.hpp"
#include "define.hpp"
#include "llvm-c/Types.h"
#include <llvm-c/Core.h>

void genFunctionBody(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                     Symbol *scope, LLVMTypeRef fn_type, LLVMValueRef func) {
  if (!node->function.undefined &&
      node->location.file.compare(codegen->source_path)) {
    LLVMBasicBlockRef prev_define = codegen->define_block;
    codegen->define_block =
        LLVMAppendBasicBlockInContext(codegen->ctx, func, "defines");
    LLVMBasicBlockRef entry =
        LLVMAppendBasicBlockInContext(codegen->ctx, func, "entry");

    LLVMBasicBlockRef prev_builder_insert_block = LLVMGetInsertBlock(builder);
    LLVMPositionBuilderAtEnd(builder, entry);

    // Prepare Arguments
    size_t param_idx = 0;

    FnABICache *abi_cache = codegen->fn_abi_cache.get(fn_type);

    // Return as argument
    bool is_ret_arg = false;
    LLVMValueRef return_ptr = nullptr;
    LLVMTypeRef return_ty = abi_cache->return_arg.type;

    if (abi_cache->return_arg.kind == ABIArgKind::Indirect) {
      is_ret_arg = true;
      return_ptr = LLVMGetParam(func, 0);
      if (abi_cache->return_arg.attribute != nullptr) {
        LLVMAddAttributeAtIndex(func, param_idx,
                                abi_cache->return_arg.attribute);
      }

      param_idx += 1;
    } else if (abi_cache->return_arg.kind != ABIArgKind::Ignore) {
      return_ptr = BuildAlloca(codegen, builder, return_ty, "return_staging");
    }

    // Prepare Parameters
    for (size_t i = 0; i < node->function.parameters.length; i++) {
      Node *key = node->function.parameters.data.ptr[i];
      char *name = (char *)codegen->allocator->alloc(key->field.name.len + 1);
      memcpy(name, key->field.name.ptr, key->field.name.len);
      name[key->field.name.len] = 0;

      LLVMTypeRef param_ty = typeToLLVM(codegen, key->value.type);
      LLVMValueRef alloca = BuildAlloca(codegen, builder, param_ty, name);
      ABIArg abi_arg = abi_cache->args.ptr[i];
      codegen->node_to_value.insert(key, alloca);

      if (abi_arg.kind == ABIArgKind::Ignore) {
        continue;
      } else if (abi_arg.kind == ABIArgKind::Direct) {
        LLVMBuildStore(builder, LLVMGetParam(func, param_idx), alloca);
      } else if (abi_arg.kind == ABIArgKind::Indirect) {
        // Dereference
        LLVMValueRef val = LLVMGetParam(func, param_idx);
        LLVMTypeRef ptr_ty = LLVMPointerType(abi_arg.type, 0);
        val = LLVMBuildLoad2(builder, ptr_ty, val, "");
        LLVMBuildStore(builder, val, alloca);
      }

      if (abi_arg.attribute != nullptr) {
        LLVMAddAttributeAtIndex(func, param_idx, abi_arg.attribute);
      }

      param_idx += 1;
    }

    // Generate body
    codegen->function_stack[codegen->function_stack_len] = {
        .def = func,
        .is_ret_arg = is_ret_arg,
        .ret_type = return_ty,
        .ret_ptr = return_ptr};
    codegen->function_defer_boundary[codegen->function_stack_len] =
        codegen->defer_stack_len;
    codegen->function_stack_len += 1;

    Symbol *fn_scope = scope->findSymbolByNode(node);
    gen(codegen, builder, node->function.body, fn_scope);
    codegen->function_stack_len -= 1;

    LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(builder);
    if (LLVMGetBasicBlockTerminator(insert_block) == nullptr) {
      injectDefer(codegen, builder, fn_scope, false);
      LLVMBuildRetVoid(builder);
    }

    // Finish define block
    LLVMPositionBuilderAtEnd(builder, codegen->define_block);
    LLVMBuildBr(builder, entry);
    codegen->define_block = prev_define;

    LLVMPositionBuilderAtEnd(builder, prev_builder_insert_block);
  }
}

LLVMValueRef prepareCallBuiltin(CodeGenModule *codegen, LLVMBuilderRef builder,
                                Node *node, Symbol *scope, Symbol *func_symbol,
                                Node *builtin_name) {
  Type *callee_type = node->call.callee->value.type;

  // Arguments
  ArrayList<LLVMValueRef> args;
  args.init(codegen->allocator, callee_type->function.arguments.length);

  for (size_t i = 0; i < node->call.arguments.length; i++) {
    Node *arg = node->call.arguments.data.ptr[i];
    args.push(gen(codegen, builder, arg, scope));
  }

  return genCallBuiltin(codegen, builder, builtin_name,
                        &node->call.callee->value, args.slice());
}

LLVMValueRef genCall(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                     Symbol *scope) {
  Type *callee_type = node->call.callee->value.type;
  bool needs_dereference = false;
  if (callee_type->kind == TypeKind::Pointer) {
    callee_type = callee_type->child;
    needs_dereference = true;
  }

  Symbol *func_symbol = nullptr;
  if (node->call.callee->value.has_data) {
    func_symbol = node->call.callee->value.data.symbol;

    // Check Builtin
    Node *parent_node = func_symbol->parent->node;
    if (parent_node->kind == NodeKind::Field &&
        parent_node->field.attributes != nullptr) {
      Node *attributes = parent_node->field.attributes;
      Node *builtin = getAttribute(attributes, "builtin");
      if (builtin != nullptr) {
        return prepareCallBuiltin(codegen, builder, node, scope, func_symbol,
                                  builtin->member.value);
      }
    }
  }

  LLVMTypeRef llvm_callee_type = typeToLLVM(codegen, callee_type);

  // Get receiver
  LLVMValueRef receiver = nullptr;
  size_t has_receiver = 0;
  if (node->call.callee->kind == NodeKind::Operator &&
      node->call.callee->_operator.opcode == Operator::MemberAccess &&
      node->call.callee->_operator.lhs->value.type->kind != TypeKind::TypeId) {
    receiver = addr(codegen, builder, node->call.callee->_operator.lhs, scope);
    has_receiver = 1;

    // Load
    Type *arg0 = callee_type->function.arguments.data.ptr[0];
    if (arg0->kind != TypeKind::Pointer) {
      receiver =
          LLVMBuildLoad2(builder, typeToLLVM(codegen, arg0), receiver, "");
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
        BuildAlloca(codegen, builder, abi_cache->return_arg.type, "return");
    args.push(ret_as_arg);
  }

  // Receiver argument
  if (receiver != nullptr) {
    args.push(receiver);
  }

  for (size_t i = 0; i < node->call.arguments.length; i++) {
    Node *arg = node->call.arguments.data.ptr[i];
    LLVMTypeRef ty = typeToLLVM(codegen, arg->value.type);
    ABIArg abi_arg = abi_cache->args.ptr[i + has_receiver];
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

    LLVMValueRef alloca = BuildAlloca(codegen, builder, abi_arg.type, "");
    LLVMBuildStore(builder, gen(codegen, builder, arg, scope), alloca);
    args.push(LLVMBuildLoad2(builder, abi_arg.type, alloca, ""));
  }

  // Build Call
  LLVMValueRef function;
  if (node->call.callee->value.has_data) {
    LLVMValueRef *opt_function = codegen->node_to_value.get(func_symbol->node);
    if (opt_function == nullptr) {
      gen(codegen, builder, func_symbol->node, func_symbol);
      opt_function = codegen->node_to_value.get(func_symbol->node);
    }

    function = *opt_function;
  } else {
    function = addr(codegen, builder, node->call.callee, scope);
  }

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
        BuildAlloca(codegen, builder, abi_cache->return_arg.type, "");
    LLVMBuildStore(builder, ret, ret_alloca);
    ret = ret_alloca;
  }
  return ret;
}
