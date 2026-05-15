#pragma once

#include "ast.hpp"
#include "tokenizer.hpp"

struct ASTParser {
  Tokenizer tokenizer;
  Node *ast;
  ArrayList<Token> comments;

  Token prev_token;
  Token cur_token;

  Allocator *allocator;

  void parse();
  bool nextToken();
};
