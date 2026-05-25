#pragma once

#include "allocator.hpp"
#include "ast.hpp"
#include "containers.hpp"
#include "symbol.hpp"
#include "llvm-c/Types.h"
#include <cstddef>

struct LoopBlocks {
  LLVMBasicBlockRef condition;
  LLVMBasicBlockRef _do;
  LLVMBasicBlockRef merge;
};

struct CodeGen {
  String source_path;
  Node *ast;
  Scope *scope;
  Allocator *allocator;

  HashMap<Scope *, LLVMTypeRef> scope_to_type;
  HashMap<Node *, LLVMValueRef> node_to_value;

  // Stacks
  LoopBlocks loop_stack[64];
  size_t loop_stack_len;

  LLVMValueRef function_stack[16];
  size_t function_stack_len;

  // LLVM Context
  LLVMContextRef ctx;
  LLVMModuleRef mod;

  void generate();
};
