#include "print.hpp"
#include "token.hpp"
#include <cstddef>
#include <ostream>
#include <string>

std::ostream &operator<<(std::ostream &os, const String &str) {
  return os.write((const char *)str.ptr, str.len);
}

std::ostream &operator<<(std::ostream &os, const SrcLoc &location) {
  return os << "[" << location.line << ":" << location.column << "]";
}

std::ostream &operator<<(std::ostream &os, const Operator &op) {
  switch (op) {
  case Operator::Assign: {
    return os << "Assign";
  }
  case Operator::Add: {
    return os << "Add";
  }
  case Operator::Sub: {
    return os << "Sub";
  }
  case Operator::Mul: {
    return os << "Mul";
  }
  case Operator::Div: {
    return os << "Div";
  }
  case Operator::Mod: {
    return os << "Mod";
  }
  case Operator::Bitwise_Or: {
    return os << "Bitwise Or";
  }
  case Operator::Bitwise_Xor: {
    return os << "Bitwise Xor";
  }
  case Operator::Bitwise_And: {
    return os << "Bitwise And";
  }
  case Operator::Bitwise_LeftShift: {
    return os << "Bitwise Left Shift";
  }
  case Operator::Bitwise_RightShift: {
    return os << "Bitwise Right Shift";
  }
  case Operator::Logical_Or: {
    return os << "Logical Or";
  }
  case Operator::Logical_And: {
    return os << "Logical And";
  }
  case Operator::EqualTo: {
    return os << "Equal To";
  }
  case Operator::NotEqualTo: {
    return os << "Not Equal To";
  }
  case Operator::LessThen: {
    return os << "Less Then";
  }
  case Operator::GreaterThen: {
    return os << "Greater Then";
  }
  case Operator::LessThenOrEqualTo: {
    return os << "Less Then or Equal To";
  }
  case Operator::GreaterThenOrEqualTo: {
    return os << "Greater Then or Equal To";
  }
  case Operator::MemberAccess: {
    return os << "Member Access";
  }
  case Operator::As: {
    return os << "As";
  }
  case Operator::Bitcast: {
    return os << "Bitcast";
  }
  case Operator::Unary_Logical_Not:
  case Operator::Unary_Bitwise_Not: {
    return os << (UnaryOperator)op;
  }
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const UnaryOperator &op) {
  switch (op) {
  case UnaryOperator::Minus: {
    return os << "Minus";
  }
  case UnaryOperator::Logical_Not: {
    return os << "Logical Not";
  }
  case UnaryOperator::Bitwise_Not: {
    return os << "Bitwise Not";
  }
  case UnaryOperator::Reference: {
    return os << "Reference";
  }
  case UnaryOperator::Dereference: {
    return os << "Dereference";
  }
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const TokenKind &kind) {
  switch (kind) {
  case TokenKind::Eof: {
    return os << "Eof";
  }
  case TokenKind::Comment: {
    return os << "Comment";
  }
  case TokenKind::Name: {
    return os << "Name";
  }
  case TokenKind::Operator: {
    return os << "Operator";
  }
  case TokenKind::Undefined: {
    return os << "Undefined";
  }
  case TokenKind::Integer: {
    return os << "Integer";
  }
  case TokenKind::Float: {
    return os << "Float";
  }
  case TokenKind::Char: {
    return os << "Char";
  }
  case TokenKind::String: {
    return os << "String";
  }
  case TokenKind::Function: {
    return os << "Function";
  }
  case TokenKind::Struct: {
    return os << "Struct";
  }
  case TokenKind::Enum: {
    return os << "Enum";
  }
  case TokenKind::Union: {
    return os << "Union";
  }
  case TokenKind::Return: {
    return os << "Return";
  }
  case TokenKind::If: {
    return os << "If";
  }
  case TokenKind::Else: {
    return os << "Else";
  }
  case TokenKind::For: {
    return os << "For";
  }
  case TokenKind::In: {
    return os << "In";
  }
  case TokenKind::Switch: {
    return os << "Switch";
  }
  case TokenKind::Break: {
    return os << "Break";
  }
  case TokenKind::Continue: {
    return os << "Continue";
  }
  case TokenKind::Defer: {
    return os << "Defer";
  }
  case TokenKind::Import: {
    return os << "Import";
  }
  case TokenKind::Comptime: {
    return os << "Comptime";
  }
  case TokenKind::Assembly: {
    return os << "Assembly";
  }
  case TokenKind::Const: {
    return os << "Const";
  }
  case TokenKind::TypeSeperator: {
    return os << "`:`";
  }
  case TokenKind::Attribute: {
    return os << "Attribute";
  }
  case TokenKind::LineDelimiter: {
    return os << "`;`";
  }
  case TokenKind::CommaDelimiter: {
    return os << "`,`";
  }
  case TokenKind::ScopeBegin: {
    return os << "`(`";
  }
  case TokenKind::ScopeEnd: {
    return os << "`)`";
  }
  case TokenKind::BlockBegin: {
    return os << "`{`";
  }
  case TokenKind::BlockEnd: {
    return os << "`}`";
  }
  case TokenKind::ArrayBegin: {
    return os << "`[`";
  }
  case TokenKind::ArrayEnd: {
    return os << "`]`";
  }
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const Token &token) {
  os << token.location << ' ' << token.kind;

  switch (token.kind) {
  case TokenKind::Comment:
  case TokenKind::Name:
  case TokenKind::String: {
    return os << " \"" << token.text << '"';
  }
  case TokenKind::Operator: {
    return os << ' ' << token._operator;
  }
  case TokenKind::Integer: {
    return os << ' ' << token.integer;
  }
  case TokenKind::Float: {
    return os << ' ' << token._float;
  }
  case TokenKind::Char: {
    return os << " '" << (char)token.integer << "'";
  }
  case TokenKind::Eof:
  case TokenKind::Undefined:
  case TokenKind::Function:
  case TokenKind::Struct:
  case TokenKind::Enum:
  case TokenKind::Union:
  case TokenKind::Return:
  case TokenKind::If:
  case TokenKind::Else:
  case TokenKind::For:
  case TokenKind::In:
  case TokenKind::Switch:
  case TokenKind::Break:
  case TokenKind::Continue:
  case TokenKind::Defer:
  case TokenKind::Import:
  case TokenKind::Comptime:
  case TokenKind::Assembly:
  case TokenKind::Const:
  case TokenKind::TypeSeperator:
  case TokenKind::Attribute:
  case TokenKind::LineDelimiter:
  case TokenKind::CommaDelimiter:
  case TokenKind::ScopeBegin:
  case TokenKind::ScopeEnd:
  case TokenKind::BlockBegin:
  case TokenKind::BlockEnd:
  case TokenKind::ArrayBegin:
  case TokenKind::ArrayEnd: {
    break;
  }
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const NodeKind &kind) {
  switch (kind) {
  case NodeKind::Name: {
    return os << "Name";
  }
  case NodeKind::UnaryOperator: {
    return os << "Unary Operator";
  }
  case NodeKind::Operator: {
    return os << "Operator";
  }
  }
  return os;
}

void print_node_impl(std::ostream &os, const Node *node, size_t depth,
                     std::string prefix) {
  std::string indent;
  indent.append(depth * 2, ' ');

  os << indent << prefix << node->kind;
  switch (node->kind) {
  case NodeKind::Name: {
    os << " \"" << node->text << "\"\n";
    break;
  }
  case NodeKind::UnaryOperator: {
    os << " `" << node->unary_operator.opcode << "`\n";
    print_node_impl(os, node->unary_operator.child, depth + 1, "Child: ");
    break;
  }
  case NodeKind::Operator: {
    os << " `" << node->_operator.opcode << "`\n";
    print_node_impl(os, node->_operator.lhs, depth + 1, "LHS: ");
    print_node_impl(os, node->_operator.rhs, depth + 1, "RHS: ");
    break;
  }
  }
}

std::ostream &operator<<(std::ostream &os, const Node &node) {
  print_node_impl(os, &node, 0, "");
  return os;
}
