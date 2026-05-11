#include "operator.hpp"
#include "print.hpp"
#include <cassert>
#include <iostream>

Precedence operatorPrecedence(Operator opcode) {
  switch (opcode) {
  case Operator::Assign: {
    return Precedence::Assign;
  }
  case Operator::Add:
  case Operator::Sub: {
    return Precedence::Add;
  }
  case Operator::Mul:
  case Operator::Div:
  case Operator::Mod: {
    return Precedence::Multiply;
  }
  case Operator::Bitwise_Or: {
    return Precedence::Bitwise_Or;
  }
  case Operator::Bitwise_Xor: {
    return Precedence::Bitwise_Xor;
  }
  case Operator::Bitwise_And: {
    return Precedence::Bitwise_And;
  }
  case Operator::Bitwise_LeftShift:
  case Operator::Bitwise_RightShift: {
    return Precedence::Shift;
  }
  case Operator::Logical_Or: {
    return Precedence::Logical_Or;
  }
  case Operator::Logical_And: {
    return Precedence::Logical_And;
  }
  case Operator::EqualTo:
  case Operator::NotEqualTo: {
    return Precedence::Equality;
  }
  case Operator::LessThen:
  case Operator::GreaterThen:
  case Operator::LessThenOrEqualTo:
  case Operator::GreaterThenOrEqualTo: {
    return Precedence::Equality_2;
  }
  case Operator::MemberAccess: {
    return Precedence::MemberAccess;
  }
  case Operator::As:
  case Operator::Bitcast: {
    return Precedence::Cast;
  }
  case Operator::Unary_Logical_Not:
  case Operator::Unary_Bitwise_Not: {
    return Precedence::Unary;
  }
  }

  std::cerr << "Unhandled precedence for operator: " << opcode << "\n";
  std::abort();
}

Associativity operatorAssociativity(Operator opcode) {
  switch (opcode) {
  case Operator::Assign:
  case Operator::Unary_Logical_Not:
  case Operator::Unary_Bitwise_Not: {
    return Associativity::Right;
  }
  case Operator::Add:
  case Operator::Sub:
  case Operator::Mul:
  case Operator::Div:
  case Operator::Mod:
  case Operator::Bitwise_Or:
  case Operator::Bitwise_Xor:
  case Operator::Bitwise_And:
  case Operator::Bitwise_LeftShift:
  case Operator::Bitwise_RightShift:
  case Operator::Logical_Or:
  case Operator::Logical_And:
  case Operator::EqualTo:
  case Operator::NotEqualTo:
  case Operator::LessThen:
  case Operator::GreaterThen:
  case Operator::LessThenOrEqualTo:
  case Operator::GreaterThenOrEqualTo:
  case Operator::MemberAccess:
  case Operator::As:
  case Operator::Bitcast: {
    return Associativity::Left;
  }
  }

  std::cerr << "Unhandled associativity for operator: " << opcode << "\n";
  std::abort();
}
