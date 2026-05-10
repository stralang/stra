#pragma once

#include "arena.hpp"
#include "token.hpp"
#include "tokenizer.hpp"

struct Node {
  SrcLoc location;
};

struct ASTParser {
  Tokenizer tokenizer;
  Arena<Node> allocator;

  void init();
  void parse();
  void deinit();
};
