#include "define.hpp"
#include "mir.hpp"
#include "mirgen.hpp"

MIRValueId addrMemberAccess(MIRGen *mirgen, Node *node, Symbol *scope) {
  MIRValueId record = addr(mirgen, node->_operator.lhs, scope);
  MIRValueId out =
      mirgen->builder.buildLookup(record, node->_operator.rhs->text);
  mirgen->builder.setSourceLocation(out, node->location);
  return out;
}

MIRValueId genAssignment(MIRGen *mirgen, Node *node, Symbol *scope) {
  MIRValueId rhs_value;
  if (node->_operator.opcode != Operator::Assign) {
    rhs_value = genBinary(mirgen, node, scope);
  } else {
    rhs_value = gen(mirgen, node->_operator.rhs, scope);
  }

  MIRValueId lhs_ptr = addr(mirgen, node->_operator.lhs, scope);
  MIRValueId out = mirgen->builder.buildStore(rhs_value, lhs_ptr);
  mirgen->builder.setSourceLocation(out, node->location);
  return out;
}

MIRValueId genUnary(MIRGen *mirgen, Node *node, Symbol *scope) {
  if (node->unary_operator.opcode == UnaryOperator::Reference) {
    MIRValueId out = addr(mirgen, node->unary_operator.child, scope);
    mirgen->builder.setSourceLocation(out, node->location);
    return out;
  }

  MIRValueId child_value = gen(mirgen, node->unary_operator.child, scope);
  MIRValueId out;

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
  default: {
    std::cerr << __FILE__ << ":" << __LINE__ << " Unhandled unary operator `"
              << (int32_t)node->unary_operator.opcode << "`\n";
    std::abort();
  }
  }

  mirgen->builder.setSourceLocation(out, node->location);
  return out;
}

MIRValueId genBinary(MIRGen *mirgen, Node *node, Symbol *scope) {
  if (node->_operator.opcode == Operator::MemberAccess) {
    MIRValueId value = addrMemberAccess(mirgen, node, scope);
    return mirgen->builder.buildLoad(value);
  }

  MIRValueId lhs_value = gen(mirgen, node->_operator.lhs, scope);
  MIRValueId rhs_value = gen(mirgen, node->_operator.rhs, scope);
  MIRValueId out;

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
  default: {
    std::cerr << __FILE__ << ":" << __LINE__ << " Unhandled binary operator `"
              << (int32_t)node->_operator.opcode << "`\n";
    std::abort();
  }
  }

  mirgen->builder.setSourceLocation(out, node->location);
  return out;
}
