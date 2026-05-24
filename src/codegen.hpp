#pragma once

#include "allocator.hpp"
#include "ast.hpp"
#include "containers.hpp"
#include "symbol.hpp"
#include "llvm-c/Types.h"

struct CodeGen {
  String source_path;
  Node *ast;
  Scope *scope;
  Allocator *allocator;

  HashMap<Scope *, LLVMTypeRef> scope_to_type;
  HashMap<Node *, LLVMValueRef> node_to_value;

  LLVMContextRef ctx;
  LLVMModuleRef mod;

  void generate();
};
