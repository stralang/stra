#include "print.hpp"

std::ostream &operator<<(std::ostream &os, const SrcLoc &location) {
  return os << "[" << location.line << ":" << location.column << "]";
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
  case TokenKind::TypeSeperator: {
    return os << "`:`";
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
  case TokenKind::Comment: {
    return os << " \"" << token.text << '"';
  }
  case TokenKind::Name: {
    return os << " \"" << token.text << '"';
  }
  case TokenKind::Keyword: {
    return os << ' ' << token.keyword;
  }
  case TokenKind::Eof:
  case TokenKind::TypeSeperator:
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
