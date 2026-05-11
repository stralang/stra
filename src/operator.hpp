#pragma once

#include <cstdint>

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

enum class Precedence : int32_t {
  Assign = 0,
  Logical_Or = 1,
  Logical_And = 2,
  Bitwise_Or = 3,
  Bitwise_Xor = 4,
  Bitwise_And = 5,
  Equality = 6,
  Equality_2 = 7,
  Shift = 8,
  Add = 9,
  Multiply = 10,
  Unary = 11,
  Cast = 12,
  MemberAccess = 13,
};

enum class Associativity : bool {
  Left = false,
  Right = true,
};

Precedence operatorPrecedence(Operator opcode);
Associativity operatorAssociativity(Operator opcode);
