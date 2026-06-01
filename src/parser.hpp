#pragma once

#include "ast.hpp"
#include "symbol.hpp"
#include "tokenizer.hpp"

struct ASTParser {
  Tokenizer tokenizer;
  Node *ast;
  Symbol *symbol;
  ArrayList<Node *> imports;

  Token prev_token;
  Token cur_token;

  Allocator *allocator;

  void parse();
  bool nextToken();
};
