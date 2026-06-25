#pragma once

#include <cstdint>

enum class Operator : int32_t {
  Assign = -1,
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

enum class UnaryOperator : int32_t {
  Minus = (int32_t)Operator::Sub,
  Logical_Not = (int32_t)Operator::Unary_Logical_Not,
  Bitwise_Not = (int32_t)Operator::Unary_Bitwise_Not,
  Reference = (int32_t)Operator::Bitwise_And,
  Dereference = (int32_t)Operator::Mul,
};

enum class Precedence : int32_t {
  Assign = 0,
  Logical_Or = 1,
  Logical_And = 2,
  Bitwise_Or = 3,
  Bitwise_Xor = 4,
  Bitwise_And = 5,
  Equality = 6,
  Relational = 7,
  Shift = 8,
  Add = 9,
  Multiply = 10,
  Cast = 11,
  Unary = 12,
  Special = 13,
  MemberAccess = 14,
};

enum class Associativity : bool {
  Left = false,
  Right = true,
};

Precedence operatorPrecedence(Operator opcode);
Associativity operatorAssociativity(Operator opcode);
