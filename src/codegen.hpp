#pragma once

#include "abi/general.hpp"
#include "allocator.hpp"
#include "ast.hpp"
#include "containers.hpp"
#include "symbol.hpp"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Types.h"
#include <cstddef>

struct LoopBlocks {
  LLVMBasicBlockRef condition;
  LLVMBasicBlockRef _do;
  LLVMBasicBlockRef merge;
};

struct FuncStackNode {
  LLVMValueRef def;
  LLVMValueRef ret_ptr;
};

struct CodeGenContext {
  LLVMContextRef ctx;
  ABI abi;

  char *target_triple;
  LLVMTargetMachineRef target_machine;
  LLVMTargetDataRef target_data;
  char *data_layout_str;

  void init();
  void deinit();
};

struct CodeGenModule {
  String source_path;
  String output_path;
  Node *ast;
  Symbol *symbol;
  Allocator *allocator;

  HashMap<Symbol *, LLVMTypeRef> scope_to_type;
  HashMap<Node *, LLVMValueRef> node_to_value;

  // Stacks
  Node *defer_stack[64];
  size_t defer_stack_len;

  LoopBlocks loop_stack[64];
  size_t loop_defer_boundary[64];
  size_t loop_stack_len;

  FuncStackNode function_stack[16];
  size_t function_defer_boundary[16];
  size_t function_stack_len;

  // LLVM Context
  TargetABI target_abi;
  size_t pointer_size;
  LLVMContextRef ctx;
  LLVMModuleRef mod;

  void generate(CodeGenContext *context);
};
