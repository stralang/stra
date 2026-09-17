#include "define.hpp"
#include "uir/uir.hpp"
#include "uirgen.hpp"

UIRValue *addrMemberAccess(UIRGen *uirgen, Node *node, Symbol *scope) {
  UIRValue *record = addr(uirgen, node->_operator.lhs, scope);
  UIRValue *out =
      uirgen->builder.buildLookupPtr(record, node->_operator.rhs->text);
  out->source_location = node->location;
  return out;
}

UIRValue *genAssignment(UIRGen *uirgen, Node *node, Symbol *scope) {
  UIRValue *rhs_value;
  if (node->_operator.opcode != Operator::Assign) {
    rhs_value = genBinary(uirgen, node, scope);
  } else {
    rhs_value = gen(uirgen, node->_operator.rhs, scope);
  }

  UIRValue *lhs_ptr = addr(uirgen, node->_operator.lhs, scope);
  UIRValue *out = uirgen->builder.buildStore(rhs_value, lhs_ptr);
  out->source_location = node->location;
  return out;
}

UIRValue *genUnary(UIRGen *uirgen, Node *node, Symbol *scope) {
  if (node->unary_operator.opcode == UnaryOperator::Reference) {
    UIRValue *out = addr(uirgen, node->unary_operator.child, scope);
    out->source_location = node->location;
    return out;
  }

  UIRValue *child_value = gen(uirgen, node->unary_operator.child, scope);
  UIRValue *out = nullptr;

  switch (node->unary_operator.opcode) {
  case UnaryOperator::Minus: {
    out = uirgen->builder.buildUnaryOp(child_value, UIROpcode::Minus);
    break;
  }
  case UnaryOperator::Logical_Not: {
    out = uirgen->builder.buildUnaryOp(child_value, UIROpcode::LogicalNot);
    break;
  }
  case UnaryOperator::Bitwise_Not: {
    out = uirgen->builder.buildUnaryOp(child_value, UIROpcode::BitwiseNot);
    break;
  }
  case UnaryOperator::Dereference: {
    out = uirgen->builder.buildLoad(child_value);
    break;
  }
  case UnaryOperator::Pointer: {
    out = uirgen->builder.buildPointer(child_value, {.ptr = nullptr});
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

UIRValue *genBinary(UIRGen *uirgen, Node *node, Symbol *scope) {
  if (node->_operator.opcode == Operator::MemberAccess) {
    UIRValue *record = addr(uirgen, node->_operator.lhs, scope);
    UIRValue *out =
        uirgen->builder.buildLookupValue(record, node->_operator.rhs->text);
    out->source_location = node->location;
    return out;
  }

  UIRValue *lhs_value = gen(uirgen, node->_operator.lhs, scope);
  UIRValue *rhs_value;
  UIRValue *out = nullptr;

  if (node->_operator.opcode == Operator::As ||
      node->_operator.opcode == Operator::Bitcast) {
    rhs_value = genComptime(uirgen, node->_operator.rhs, scope);
  } else {
    rhs_value = gen(uirgen, node->_operator.rhs, scope);
  }

  switch (node->_operator.opcode) {
  case Operator::Add: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::Add);
    break;
  }
  case Operator::Sub: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::Sub);
    break;
  }
  case Operator::Mul: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::Mul);
    break;
  }
  case Operator::Div: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::Div);
    break;
  }
  case Operator::Mod: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::Mod);
    break;
  }
  case Operator::Bitwise_Or: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::Or);
    break;
  }
  case Operator::Bitwise_Xor: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::Xor);
    break;
  }
  case Operator::Bitwise_And: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::And);
    break;
  }
  case Operator::Bitwise_LeftShift: {
    out =
        uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::LeftShift);
    break;
  }
  case Operator::Bitwise_RightShift: {
    out =
        uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::RightShift);
    break;
  }
  case Operator::Logical_Or: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::Or);
    break;
  }
  case Operator::Logical_And: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::And);
    break;
  }
  case Operator::EqualTo: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::EqualTo);
    break;
  }
  case Operator::NotEqualTo: {
    out =
        uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::NotEqualTo);
    break;
  }
  case Operator::LessThen: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::LessThen);
    break;
  }
  case Operator::GreaterThen: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value,
                                     UIROpcode::GreaterThen);
    break;
  }
  case Operator::LessThenOrEqualTo: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value,
                                     UIROpcode::LessThenOrEqualTo);
    break;
  }
  case Operator::GreaterThenOrEqualTo: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value,
                                     UIROpcode::GreaterThenOrEqualTo);
    break;
  }
  case Operator::As: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::As);
    break;
  }
  case Operator::Bitcast: {
    out = uirgen->builder.buildBinOp(lhs_value, rhs_value, UIROpcode::Bitcast);
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
