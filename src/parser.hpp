#pragma once

#include "ast.hpp"
#include "containers.hpp"
#include "symbol.hpp"
#include "tokenizer.hpp"
#include "types.hpp"
#include <cstddef>

struct ASTParser {
  Tokenizer tokenizer;
  Node *ast;
  Symbol *symbol;
  ArrayList<Node *> imports;

  Token prev_token;
  Token cur_token;

  size_t error_count = 0;
  void (*error_func)(SrcLoc srcloc, String msg);

  TypeCache *type_cache;
  Allocator *allocator;

  void parse();
  bool nextToken();
};
