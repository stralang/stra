#pragma once

#include "token.hpp"
#include "types.hpp"
#include <cstddef>

struct Tokenizer {
  String path;
  String source;

  size_t index;
  size_t line;
  size_t column;

  void init();
  void deinit();

  Token next();

  char nextChar();
};
