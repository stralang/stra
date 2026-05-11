#pragma once

#include "operator.hpp"
#include "types.hpp"
#include <cstdint>

struct SrcLoc {
  String file;
  size_t index;
  size_t line;
  size_t column;
};

enum class TokenKind : uint32_t {
  Eof,
  Comment,
  Name,
  Operator,
  Undefined,

  Integer,
  Float,
  Char,
  String,

  Function,
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
  Const,

  TypeSeperator,
  Attribute,
  LineDelimiter,
  CommaDelimiter,
  ScopeBegin,
  ScopeEnd,
  BlockBegin,
  BlockEnd,
  ArrayBegin,
  ArrayEnd,
};

struct Token {
  TokenKind kind;
  union {
    String text;
    Operator _operator;
    int64_t integer;
    double _float;
  };
  SrcLoc location;
};
