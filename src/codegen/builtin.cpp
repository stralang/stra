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
                             Slice<LLVMValueRef> args) {
  LLVMValueRef fn =
      codegen->function_stack[codegen->function_stack_len - 1].def;
  LLVMValueRef order = args[1];

  // Create Switch
  LLVMBasicBlockRef else_block =
      LLVMCreateBasicBlockInContext(codegen->ctx, "");
  LLVMValueRef _switch =
      LLVMBuildSwitch(builder, order, else_block,
                      atomic_ordering_count * atomic_ordering_count);

  // Else
  LLVMAppendExistingBasicBlock(fn, else_block);
  LLVMBuildUnreachable(builder);

  // Merge
  LLVMBasicBlockRef merge_block =
      LLVMAppendBasicBlockInContext(codegen->ctx, fn, "atomic_merge");
  LLVMPositionBuilderAtEnd(builder, merge_block);

  // Result
  LLVMTypeRef result_type = LLVMGetElementType(LLVMTypeOf(args[0]));
  LLVMValueRef out = LLVMBuildPhi(builder, result_type, "atomic_result");

  // Casses
  for (size_t i = 0; i < atomic_ordering_count; i++) {
    LLVMBasicBlockRef case_block =
        LLVMAppendBasicBlockInContext(codegen->ctx, fn, "");
    LLVMPositionBuilderAtEnd(builder, case_block);

    LLVMValueRef val =
        LLVMConstInt(LLVMInt8TypeInContext(codegen->ctx), i, false);
    LLVMAddCase(_switch, val, case_block);

    LLVMValueRef load = LLVMBuildLoad2(builder, nullptr, args[0], "");
    LLVMSetOrdering(load, atomic_ordering[i]);
    LLVMAddIncoming(out, &load, &case_block, 1);

    LLVMBuildBr(builder, merge_block);
  }

  LLVMAppendExistingBasicBlock(fn, merge_block);
  LLVMPositionBuilderAtEnd(builder, merge_block);

  return out;
}

void buildAtomicStore(CodeGenModule *codegen, LLVMBuilderRef builder,
                      Slice<LLVMValueRef> args) {
  LLVMValueRef fn =
      codegen->function_stack[codegen->function_stack_len - 1].def;
  LLVMValueRef order = args[1];

  // Create Switch
  LLVMBasicBlockRef else_block =
      LLVMCreateBasicBlockInContext(codegen->ctx, "");
  LLVMValueRef _switch =
      LLVMBuildSwitch(builder, order, else_block,
                      atomic_ordering_count * atomic_ordering_count);

  // Else
  LLVMAppendExistingBasicBlock(fn, else_block);
  LLVMBuildUnreachable(builder);

  // Merge
  LLVMBasicBlockRef merge_block =
      LLVMAppendBasicBlockInContext(codegen->ctx, fn, "atomic_merge");
  LLVMPositionBuilderAtEnd(builder, merge_block);

  // Casses
  for (size_t i = 0; i < atomic_ordering_count; i++) {
    LLVMBasicBlockRef case_block =
        LLVMAppendBasicBlockInContext(codegen->ctx, fn, "");
    LLVMPositionBuilderAtEnd(builder, case_block);

    LLVMValueRef val =
        LLVMConstInt(LLVMInt8TypeInContext(codegen->ctx), i, false);
    LLVMAddCase(_switch, val, case_block);

    LLVMValueRef store = LLVMBuildStore(builder, args[1], args[0]);
    LLVMSetOrdering(store, atomic_ordering[i]);

    LLVMBuildBr(builder, merge_block);
  }

  LLVMAppendExistingBasicBlock(fn, merge_block);
  LLVMPositionBuilderAtEnd(builder, merge_block);
}

LLVMValueRef buildAtomicCompareExchange(CodeGenModule *codegen,
                                        LLVMBuilderRef builder,
                                        Slice<LLVMValueRef> args) {
  LLVMValueRef fn =
      codegen->function_stack[codegen->function_stack_len - 1].def;

  // Calculate order
  LLVMValueRef shift_4 =
      LLVMConstInt(LLVMInt8TypeInContext(codegen->ctx), 4, false);
  LLVMValueRef order = LLVMBuildShl(builder, args[3], shift_4, "");
  order = LLVMBuildOr(builder, order, args[4], "");

  // Create Switch

  LLVMBasicBlockRef else_block =
      LLVMCreateBasicBlockInContext(codegen->ctx, "");
  LLVMValueRef _switch =
      LLVMBuildSwitch(builder, order, else_block,
                      atomic_ordering_count * atomic_ordering_count);

  // Else
  LLVMAppendExistingBasicBlock(fn, else_block);
  LLVMBuildUnreachable(builder);

  // Merge
  LLVMBasicBlockRef merge_block =
      LLVMAppendBasicBlockInContext(codegen->ctx, fn, "atomic_merge");
  LLVMPositionBuilderAtEnd(builder, merge_block);

  // Result
  LLVMTypeRef result_type = LLVMGetElementType(LLVMTypeOf(args[0]));
  LLVMValueRef out = LLVMBuildPhi(builder, result_type, "atomic_result");

  // Casses
  for (size_t i = 0; i < atomic_ordering_count; i++) {
    for (size_t k = 0; k < atomic_ordering_count; k++) {
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
      LLVMValueRef result =
          LLVMBuildAtomicCmpXchg(builder, args[0], args[1], args[2],
                                 atomic_ordering[i], atomic_ordering[k], false);
      LLVMAddIncoming(out, &result, &case_block, 1);

      // Exit
      LLVMBuildBr(builder, merge_block);
    }
  }

  LLVMAppendExistingBasicBlock(fn, merge_block);
  LLVMPositionBuilderAtEnd(builder, merge_block);

  return out;
}

LLVMValueRef genCallBuiltin(CodeGenModule *codegen, LLVMBuilderRef builder,
                            Type *callee_type, Slice<LLVMValueRef> args) {
  assert(callee_type->kind == TypeKind::Function &&
         "Callee must be a function");

  Symbol *fn_scope = callee_type->function.scope;
  Symbol *parent_scope = fn_scope->parent;
  if (parent_scope == nullptr || parent_scope->node->kind != NodeKind::Field) {
    std::cerr << "Cannot generate builtin function from floating function\n";
    return nullptr;
  }

  Node *field_node = parent_scope->node;
  String name = field_node->field.name;

  if (name.compare("atomicLoad")) {
    return buildAtomicLoad(codegen, builder, args);
  } else if (name.compare("atomicStore")) {
    buildAtomicStore(codegen, builder, args);
    return nullptr;
  } else if (name.compare("atomicCompareExchange")) {
    return buildAtomicCompareExchange(codegen, builder, args);
  }

  std::cerr << "Unsupported builtin function in codegen `" << name
            << "`. Aborting\n";
  std::abort();
}
