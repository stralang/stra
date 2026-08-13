#include "define.hpp"
#include "mir.hpp"
#include "mirgen.hpp"

MIRValue *addrMemberAccess(MIRGen *mirgen, Node *node, Symbol *scope) {
  // TODO: Member Access
  std::cerr << __FILE__ << ":" << __LINE__ << " TODO: Member Access";
  return nullptr;
}

MIRValue *genAssignment(MIRGen *mirgen, Node *node, Symbol *scope) {
  MIRValue *rhs_value;
  if (node->_operator.opcode != Operator::Assign) {
    rhs_value = genBinary(mirgen, node, scope);
  } else {
    rhs_value = gen(mirgen, node->_operator.rhs, scope);
  }

  MIRValue *lhs_ptr = addr(mirgen, node->_operator.lhs, scope);
  return mirgen->builder.buildStore(rhs_value, lhs_ptr);
}

MIRValue *genUnary(MIRGen *mirgen, Node *node, Symbol *scope) {
  if (node->unary_operator.opcode == UnaryOperator::Reference) {
    return addr(mirgen, node->unary_operator.child, scope);
  }

  MIRValue *child_value = gen(mirgen, node->unary_operator.child, scope);

  switch (node->unary_operator.opcode) {
  case UnaryOperator::Minus: {
    return mirgen->builder.buildUnaryOp(child_value, MIROpcode::Minus);
  }
  case UnaryOperator::Logical_Not: {
    return mirgen->builder.buildUnaryOp(child_value, MIROpcode::LogicalNot);
  }
  case UnaryOperator::Bitwise_Not: {
    return mirgen->builder.buildUnaryOp(child_value, MIROpcode::BitwiseNot);
  }
  case UnaryOperator::Dereference: {
    return child_value;
  }
  }

  std::cerr << __FILE__ << ":" << __LINE__ << " Unhandled unary operator `"
            << (int32_t)node->unary_operator.opcode << "`\n";
  return nullptr;
}

MIRValue *genBinary(MIRGen *mirgen, Node *node, Symbol *scope) {
  if (node->_operator.opcode == Operator::MemberAccess) {
    MIRValue *value = addrMemberAccess(mirgen, node, scope);
    return mirgen->builder.buildLoad(value);
  }

  MIRValue *lhs_value = gen(mirgen, node->_operator.lhs, scope);
  MIRValue *rhs_value = gen(mirgen, node->_operator.rhs, scope);

  switch (node->_operator.opcode) {
  case Operator::Add: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Add);
  }
  case Operator::Sub: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Sub);
  }
  case Operator::Mul: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Mul);
  }
  case Operator::Div: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Div);
  }
  case Operator::Mod: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Mod);
  }
  case Operator::Bitwise_Or: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Or);
  }
  case Operator::Bitwise_Xor: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Xor);
  }
  case Operator::Bitwise_And: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::And);
  }
  case Operator::Bitwise_LeftShift: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value,
                                      MIROpcode::LeftShift);
  }
  case Operator::Bitwise_RightShift: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value,
                                      MIROpcode::RightShift);
  }
  case Operator::Logical_Or: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Or);
  }
  case Operator::Logical_And: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::And);
  }
  case Operator::EqualTo: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::EqualTo);
  }
  case Operator::NotEqualTo: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value,
                                      MIROpcode::NotEqualTo);
  }
  case Operator::LessThen: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value,
                                      MIROpcode::LessThen);
  }
  case Operator::GreaterThen: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value,
                                      MIROpcode::GreaterThen);
  }
  case Operator::LessThenOrEqualTo: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value,
                                      MIROpcode::LessThenOrEqualTo);
  }
  case Operator::GreaterThenOrEqualTo: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value,
                                      MIROpcode::GreaterThenOrEqualTo);
  }
  case Operator::As: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::As);
  }
  case Operator::Bitcast: {
    return mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Bitcast);
  }
  }

  std::cerr << __FILE__ << ":" << __LINE__ << " Unhandled binary operator `"
            << (int32_t)node->_operator.opcode << "`\n";
  return nullptr;
}
