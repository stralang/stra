#pragma once

#include "types.hpp"

struct SrcLoc {
  String file;
  size_t index;
  size_t line;
  size_t column;
};

enum TokenKind {
  Eof,
  Comment,
  Name,
};

struct Token {
  TokenKind kind;
  union {
    String text;
  };
  SrcLoc location;
};
