#pragma once

#include "containers.hpp"
#include "operator.hpp"
#include "srcloc.hpp"
#include <cstdint>

enum class TokenKind : uint32_t {
  Eof,
  Comment,
  Name,
  Eq,
  Assignment,
  Operator,
  Undefined,
  Case,
  RangeLessThen,
  RangeEqualTo,

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
