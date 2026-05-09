#include "print.hpp"
#include "token.hpp"

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

std::ostream &operator<<(std::ostream &os, const Keyword &keyword) {
  switch (keyword) {
  case Keyword::Function: {
    return os << "Function";
  }
  case Keyword::Struct: {
    return os << "Struct";
  }
  case Keyword::Enum: {
    return os << "Enum";
  }
  case Keyword::Union: {
    return os << "Union";
  }
  case Keyword::Return: {
    return os << "Return";
  }
  case Keyword::If: {
    return os << "If";
  }
  case Keyword::Else: {
    return os << "Else";
  }
  case Keyword::For: {
    return os << "For";
  }
  case Keyword::In: {
    return os << "In";
  }
  case Keyword::Switch: {
    return os << "Switch";
  }
  case Keyword::Break: {
    return os << "Break";
  }
  case Keyword::Continue: {
    return os << "Continue";
  }
  case Keyword::Defer: {
    return os << "Defer";
  }
  case Keyword::Import: {
    return os << "Import";
  }
  case Keyword::Comptime: {
    return os << "Comptime";
  }
  case Keyword::Assembly: {
    return os << "Assembly";
  }
  case Keyword::Const: {
    return os << "Const";
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
  case TokenKind::Keyword: {
    return os << "Keyword";
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
  case TokenKind::Keyword: {
    return os << ' ' << token.keyword;
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

std::ostream &operator<<(std::ostream &os, const String &str) {
  return os.write((const char *)str.ptr, str.len);
}
