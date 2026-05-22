#pragma once

#include "allocator.hpp"
#include "ast.hpp"
#include "symbol.hpp"
#include "llvm-c/Types.h"

struct CodeGen {
  String path;
  Node *ast;
  Scope *scope;
  Allocator *allocator;

  LLVMModuleRef mod;

  void generate();
};
