#pragma once

#include "types.hpp"
#include <cstdint>

struct SrcLoc {
  String file;
  size_t index;
  size_t line;
  size_t column;
};

enum class Operator : uint32_t {
  Assign,
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  Bitwise_Or,
  Bitwise_Xor,
  Bitwise_And,
  Bitwise_LeftShift,
  Bitwise_RightShift,
  Logical_Or,
  Logical_And,
  EqualTo,
  NotEqualTo,
  LessThen,
  GreaterThen,
  LessThenOrEqualTo,
  GreaterThenOrEqualTo,
  MemberAccess,
  As,
  Bitcast,
  Unary_Logical_Not,
  Unary_Bitwise_Not,
};

enum class UnaryOperator : uint32_t {
  Minus = (uint32_t)Operator::Sub,
  Logical_Not = (uint32_t)Operator::Unary_Logical_Not,
  Bitwise_Not = (uint32_t)Operator::Unary_Bitwise_Not,
  Reference = (uint32_t)Operator::Bitwise_And,
  Dereference = (uint32_t)Operator::Mul,
};

enum class Keyword : uint32_t {
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

enum class TokenKind : uint32_t {
  Eof,
  Comment,
  Name,
  Keyword,
  Operator,
  Undefined,

  Integer,
  Float,
  Char,
  String,

  TypeSeperator,
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
    Keyword keyword;
    Operator _operator;
    int64_t integer;
    double _float;
  };
  SrcLoc location;
};
