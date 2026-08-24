#include "define.hpp"
#include "mir.hpp"
#include "mirgen.hpp"

MIRValue *addrMemberAccess(MIRGen *mirgen, Node *node, Symbol *scope) {
  MIRValue *record = addr(mirgen, node->_operator.lhs, scope);
  MIRValue *out =
      mirgen->builder.buildMemberAccess(record, node->_operator.rhs->text);
  out->source_location = node->location;
  return out;
}

MIRValue *genAssignment(MIRGen *mirgen, Node *node, Symbol *scope) {
  MIRValue *rhs_value;
  if (node->_operator.opcode != Operator::Assign) {
    rhs_value = genBinary(mirgen, node, scope);
  } else {
    rhs_value = gen(mirgen, node->_operator.rhs, scope);
  }

  MIRValue *lhs_ptr = addr(mirgen, node->_operator.lhs, scope);
  MIRValue *out = mirgen->builder.buildStore(rhs_value, lhs_ptr);
  out->source_location = node->location;
  return out;
}

MIRValue *genUnary(MIRGen *mirgen, Node *node, Symbol *scope) {
  if (node->unary_operator.opcode == UnaryOperator::Reference) {
    MIRValue *out = addr(mirgen, node->unary_operator.child, scope);
    out->source_location = node->location;
    return out;
  }

  MIRValue *child_value = gen(mirgen, node->unary_operator.child, scope);
  MIRValue *out = nullptr;

  switch (node->unary_operator.opcode) {
  case UnaryOperator::Minus: {
    out = mirgen->builder.buildUnaryOp(child_value, MIROpcode::Minus);
    break;
  }
  case UnaryOperator::Logical_Not: {
    out = mirgen->builder.buildUnaryOp(child_value, MIROpcode::LogicalNot);
    break;
  }
  case UnaryOperator::Bitwise_Not: {
    out = mirgen->builder.buildUnaryOp(child_value, MIROpcode::BitwiseNot);
    break;
  }
  case UnaryOperator::Dereference: {
    out = child_value;
    break;
  }
  }

  if (out != nullptr) {
    out->source_location = node->location;
    return out;
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
  MIRValue *out = nullptr;

  switch (node->_operator.opcode) {
  case Operator::Add: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Add);
    break;
  }
  case Operator::Sub: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Sub);
    break;
  }
  case Operator::Mul: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Mul);
    break;
  }
  case Operator::Div: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Div);
    break;
  }
  case Operator::Mod: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Mod);
    break;
  }
  case Operator::Bitwise_Or: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Or);
    break;
  }
  case Operator::Bitwise_Xor: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Xor);
    break;
  }
  case Operator::Bitwise_And: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::And);
    break;
  }
  case Operator::Bitwise_LeftShift: {
    out =
        mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::LeftShift);
    break;
  }
  case Operator::Bitwise_RightShift: {
    out =
        mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::RightShift);
    break;
  }
  case Operator::Logical_Or: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Or);
    break;
  }
  case Operator::Logical_And: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::And);
    break;
  }
  case Operator::EqualTo: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::EqualTo);
    break;
  }
  case Operator::NotEqualTo: {
    out =
        mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::NotEqualTo);
    break;
  }
  case Operator::LessThen: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::LessThen);
    break;
  }
  case Operator::GreaterThen: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value,
                                     MIROpcode::GreaterThen);
    break;
  }
  case Operator::LessThenOrEqualTo: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value,
                                     MIROpcode::LessThenOrEqualTo);
    break;
  }
  case Operator::GreaterThenOrEqualTo: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value,
                                     MIROpcode::GreaterThenOrEqualTo);
    break;
  }
  case Operator::As: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::As);
    break;
  }
  case Operator::Bitcast: {
    out = mirgen->builder.buildBinOp(lhs_value, rhs_value, MIROpcode::Bitcast);
    break;
  }
  }

  if (out != nullptr) {
    out->source_location = node->location;
    return out;
  }

  std::cerr << __FILE__ << ":" << __LINE__ << " Unhandled binary operator `"
            << (int32_t)node->_operator.opcode << "`\n";
  return nullptr;
}
