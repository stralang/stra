#include "../print.hpp"
#include "../symbol.hpp"
#include "codegen.hpp"
#include "define.hpp"
#include "llvm-c/Types.h"
#include <cassert>
#include <iostream>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

const size_t atomic_ordering_count = 6;
const LLVMAtomicOrdering atomic_ordering[atomic_ordering_count] = {
    LLVMAtomicOrderingUnordered,      LLVMAtomicOrderingMonotonic,
    LLVMAtomicOrderingAcquire,        LLVMAtomicOrderingRelease,
    LLVMAtomicOrderingAcquireRelease, LLVMAtomicOrderingSequentiallyConsistent,
};

LLVMValueRef buildAtomicLoad(CodeGenModule *codegen, LLVMBuilderRef builder,
                             Slice<LLVMValueRef> args,
                             Slice<Type *> arg_types) {
  LLVMValueRef fn =
      codegen->function_stack[codegen->function_stack_len - 1].def;
  LLVMValueRef order = args[1];

  // Result
  LLVMTypeRef result_type = typeToLLVM(codegen, arg_types[0]->child);
  LLVMValueRef out =
      BuildAlloca(codegen, builder, result_type, "atomic_result");

  // Create Switch
  LLVMBasicBlockRef merge_block =
      LLVMCreateBasicBlockInContext(codegen->ctx, "atomic_merge");
  LLVMBasicBlockRef else_block =
      LLVMCreateBasicBlockInContext(codegen->ctx, "invalid_order");
  LLVMValueRef _switch =
      LLVMBuildSwitch(builder, order, else_block, atomic_ordering_count);

  // Else
  LLVMAppendExistingBasicBlock(fn, else_block);
  LLVMPositionBuilderAtEnd(builder, else_block);
  LLVMBuildUnreachable(builder);

  // Casses
  for (size_t i = 0; i < atomic_ordering_count; i++) {
    LLVMAtomicOrdering ordering = atomic_ordering[i];
    if (ordering == LLVMAtomicOrderingRelease ||
        ordering == LLVMAtomicOrderingAcquireRelease) {
      continue;
    }

    LLVMBasicBlockRef case_block =
        LLVMAppendBasicBlockInContext(codegen->ctx, fn, "order_case");
    LLVMPositionBuilderAtEnd(builder, case_block);

    LLVMValueRef load = LLVMBuildLoad2(builder, result_type, args[0], "");
    LLVMSetOrdering(load, ordering);
    LLVMBuildStore(builder, load, out);

    LLVMBuildBr(builder, merge_block);

    LLVMValueRef val =
        LLVMConstInt(LLVMInt8TypeInContext(codegen->ctx), i, false);
    LLVMAddCase(_switch, val, case_block);
  }

  // Merge
  LLVMAppendExistingBasicBlock(fn, merge_block);
  LLVMPositionBuilderAtEnd(builder, merge_block);
  LLVMPositionBuilderAtEnd(builder, merge_block);

  return out;
}

void buildAtomicStore(CodeGenModule *codegen, LLVMBuilderRef builder,
                      Slice<LLVMValueRef> args, Slice<Type *> arg_types) {
  LLVMValueRef fn =
      codegen->function_stack[codegen->function_stack_len - 1].def;
  LLVMValueRef order = args[2];

  // Create Switch
  LLVMBasicBlockRef merge_block =
      LLVMCreateBasicBlockInContext(codegen->ctx, "atomic_merge");
  LLVMBasicBlockRef else_block =
      LLVMCreateBasicBlockInContext(codegen->ctx, "invalid_order");
  LLVMValueRef _switch =
      LLVMBuildSwitch(builder, order, else_block, atomic_ordering_count);

  // Else
  LLVMAppendExistingBasicBlock(fn, else_block);
  LLVMPositionBuilderAtEnd(builder, else_block);
  LLVMBuildUnreachable(builder);

  // Casses
  for (size_t i = 0; i < atomic_ordering_count; i++) {
    LLVMAtomicOrdering ordering = atomic_ordering[i];
    if (ordering == LLVMAtomicOrderingAcquire ||
        ordering == LLVMAtomicOrderingAcquireRelease) {
      continue;
    }

    LLVMBasicBlockRef case_block =
        LLVMAppendBasicBlockInContext(codegen->ctx, fn, "");
    LLVMPositionBuilderAtEnd(builder, case_block);

    LLVMValueRef val =
        LLVMConstInt(LLVMInt8TypeInContext(codegen->ctx), i, false);
    LLVMAddCase(_switch, val, case_block);

    LLVMValueRef store = LLVMBuildStore(builder, args[1], args[0]);
    LLVMSetOrdering(store, ordering);

    LLVMBuildBr(builder, merge_block);
  }

  LLVMAppendExistingBasicBlock(fn, merge_block);
  LLVMPositionBuilderAtEnd(builder, merge_block);
}

LLVMValueRef buildAtomicCompareExchange(CodeGenModule *codegen,
                                        LLVMBuilderRef builder,
                                        Slice<LLVMValueRef> args,
                                        Slice<Type *> arg_types) {
  LLVMValueRef fn =
      codegen->function_stack[codegen->function_stack_len - 1].def;

  // Calculate order
  LLVMValueRef shift_4 =
      LLVMConstInt(LLVMInt8TypeInContext(codegen->ctx), 4, false);
  LLVMValueRef order = LLVMBuildShl(builder, args[3], shift_4, "");
  order = LLVMBuildOr(builder, order, args[4], "");

  // Result
  LLVMTypeRef result_elem_types[2];
  result_elem_types[0] = typeToLLVM(codegen, arg_types[0]->child);
  result_elem_types[1] = LLVMInt1TypeInContext(codegen->ctx);

  LLVMTypeRef result_type =
      LLVMStructTypeInContext(codegen->ctx, result_elem_types, 2, false);
  LLVMValueRef out =
      BuildAlloca(codegen, builder, result_type, "atomic_result");

  // Create Switch
  LLVMBasicBlockRef merge_block =
      LLVMCreateBasicBlockInContext(codegen->ctx, "atomic_merge");
  LLVMBasicBlockRef else_block =
      LLVMCreateBasicBlockInContext(codegen->ctx, "invalid_order");
  LLVMValueRef _switch =
      LLVMBuildSwitch(builder, order, else_block,
                      atomic_ordering_count * atomic_ordering_count);

  // Else
  LLVMAppendExistingBasicBlock(fn, else_block);
  LLVMPositionBuilderAtEnd(builder, else_block);
  LLVMBuildUnreachable(builder);

  // Casses
  for (size_t i = 0; i < atomic_ordering_count; i++) {
    LLVMAtomicOrdering success_order = atomic_ordering[i];
    if (success_order < LLVMAtomicOrderingMonotonic) {
      continue;
    }

    for (size_t k = 0; k < atomic_ordering_count; k++) {
      LLVMAtomicOrdering fail_order = atomic_ordering[k];
      if (fail_order < LLVMAtomicOrderingMonotonic ||
          fail_order == LLVMAtomicOrderingRelease ||
          fail_order == LLVMAtomicOrderingAcquireRelease ||
          fail_order > success_order) {
        continue;
      }

      LLVMBasicBlockRef case_block =
          LLVMAppendBasicBlockInContext(codegen->ctx, fn, "");
      LLVMPositionBuilderAtEnd(builder, case_block);

      // Calculate order
      LLVMValueRef success_value =
          LLVMConstInt(LLVMInt8TypeInContext(codegen->ctx), i, false);
      LLVMValueRef fail_value =
          LLVMConstInt(LLVMInt8TypeInContext(codegen->ctx), k, false);
      success_value = LLVMBuildShl(builder, success_value, shift_4, "");

      LLVMValueRef val = LLVMBuildOr(builder, success_value, fail_value, "");
      LLVMAddCase(_switch, val, case_block);

      // Action
      LLVMValueRef result = LLVMBuildAtomicCmpXchg(
          builder, args[0], args[1], args[2], success_order, fail_order, false);
      LLVMBuildStore(builder, result, out);

      // Exit
      LLVMBuildBr(builder, merge_block);
    }
  }

  // Merge
  LLVMAppendExistingBasicBlock(fn, merge_block);
  LLVMPositionBuilderAtEnd(builder, merge_block);

  return out;
}

// TODO: Should the function be created instead?
LLVMValueRef genCallBuiltin(CodeGenModule *codegen, LLVMBuilderRef builder,
                            Type *callee_type, Slice<LLVMValueRef> args) {
  assert(callee_type->kind == TypeKind::Function &&
         "Callee must be a function");

  Slice<Type *> arg_types = callee_type->function.arguments.slice();
  Symbol *fn_scope = callee_type->function.scope;
  Symbol *parent_scope = fn_scope->parent;
  if (parent_scope == nullptr || parent_scope->node->kind != NodeKind::Field) {
    std::cerr << "Cannot generate builtin function from floating function\n";
    return nullptr;
  }

  Node *field_node = parent_scope->node;
  String name = field_node->field.name;

  if (name.compare("atomicLoad")) {
    return buildAtomicLoad(codegen, builder, args, arg_types);
  } else if (name.compare("atomicStore")) {
    buildAtomicStore(codegen, builder, args, arg_types);
    return nullptr;
  } else if (name.compare("atomicCompareExchange")) {
    return buildAtomicCompareExchange(codegen, builder, args, arg_types);
  }

  std::cerr << "Unsupported builtin function in codegen `" << name
            << "`. Aborting\n";
  std::abort();
}
