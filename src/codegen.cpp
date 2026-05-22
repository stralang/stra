#include "codegen.hpp"
#include "llvm-c/Core.h"
#include <cstring>
#include <iostream>

void CodeGen::generate() {
  char *name = (char *)allocator->alloc(sizeof(this->path.len + 1));
  memcpy(name, this->path.ptr, this->path.len);
  *(name + this->path.len) = 0;

  this->mod = LLVMModuleCreateWithName(name);

  char *text = LLVMPrintModuleToString(this->mod);
  std::cout << text;
}
