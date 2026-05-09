#pragma once

#include "types.hpp"

struct SrcLoc {
  String file;
  size_t index;
  size_t line;
  size_t column;
};

enum class Keyword {
  Function = 0,
  Struct,
  Enum,
  Union,
  Return,
  If,
  Else,
  For,
  In,
  Switch,
  Break,
  Continue,
  Defer,
  Import,
  Comptime,
  Assembly,
};

enum class TokenKind {
  Eof,
  Comment,
  Name,
  Keyword,
};

struct Token {
  TokenKind kind;
  union {
    String text;
    Keyword keyword;
  };
  SrcLoc location;
};
