#include "tokenizer.hpp"
#include "token.hpp"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <iostream>
#include <map>
#include <string>

std::map<std::string, Keyword> keyword_mapping = {
    {"fn", Keyword::Function},
    {"struct", Keyword::Struct},
    {"enum", Keyword::Enum},
    {"union", Keyword::Union},
    {"return", Keyword::Return},
    {"if", Keyword::If},
    {"else", Keyword::Else},
    {"for", Keyword::For},
    {"in", Keyword::In},
    {"switch", Keyword::Switch},
    {"break", Keyword::Break},
    {"continue", Keyword::Continue},
    {"defer", Keyword::Defer},
    {"import", Keyword::Import},
    {"comptime", Keyword::Comptime},
    {"asm", Keyword::Assembly},
};

std::map<std::string, Operator> operator_keyword_mapping = {
    {"as", Operator::As},
    {"bitcast", Operator::Bitcast},
};

void Tokenizer::init() {
  std::string filepath((const char *)this->path.ptr, this->path.len);
  std::ifstream file(filepath);
  if (!file) {
    assert("Failed to open file" && 0);
  }

  file.seekg(0, std::ios::end);
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  char *text = (char *)malloc(sizeof(char) * size);
  if (file.read(text, size)) {
    this->source.ptr = (uint8_t *)text;
    this->source.len = size;
    this->index = 0;
    this->line = 1;
    this->column = 1;
  } else {
    free(text);
    assert("Failed to read file" && 0);
  }

  file.close();
}

void Tokenizer::deinit() { free((void *)this->source.ptr); }

Token Tokenizer::next() {
  if (this->index >= this->source.len) {
    return Token{.kind = TokenKind::Eof};
  }

  char c = this->source[this->index];

  // Skip whitespace
  while (c == ' ' || c == '\n' || c == '\t') {
    c = this->nextChar();
  }

  Token token;
  token.kind = TokenKind::Eof;
  token.location = SrcLoc{
      .file = this->path,
      .index = this->index,
      .line = this->line,
      .column = this->column,
  };

  if (c == -1) {
    token.kind = TokenKind::Eof;
    return token;
  }

  // Parse Comments
  if (c == '/' && this->index + 1 < this->source.len) {
    char peek = this->source.ptr[this->index + 1];
    if (peek == '/' || peek == '*') {
      this->nextChar();
      c = this->nextChar();

      // Skip first space
      if (c == ' ') {
        c = this->nextChar();
      }

      // Find end
      bool multiline = peek == '*';
      size_t start = this->index;
      while (multiline || c != '\n') {
        c = this->nextChar();
        if (!multiline || c != '*') {
          continue;
        }

        c = this->nextChar();
        if (c == '/') {
          break;
        }
      }

      // Remove multi-line comment markers
      size_t end = this->index - 1;
      if (multiline) {
        end -= 1;
      }

      // Return comment token
      token.kind = TokenKind::Comment;
      token.text = this->source.range(start, end);
      this->nextChar();
      return token;
    }
  }

  // Parse Symbols and Operators
  switch (c) {
    // Symbols
  case ':': {
    token.kind = TokenKind::TypeSeperator;
    this->nextChar();
    return token;
  }
  case ';': {
    token.kind = TokenKind::LineDelimiter;
    this->nextChar();
    return token;
  }
  case ',': {
    token.kind = TokenKind::CommaDelimiter;
    this->nextChar();
    return token;
  }
  case '(': {
    token.kind = TokenKind::ScopeBegin;
    this->nextChar();
    return token;
  }
  case ')': {
    token.kind = TokenKind::ScopeEnd;
    this->nextChar();
    return token;
  }
  case '{': {
    token.kind = TokenKind::BlockBegin;
    this->nextChar();
    return token;
  }
  case '}': {
    token.kind = TokenKind::BlockEnd;
    this->nextChar();
    return token;
  }
  case '[': {
    token.kind = TokenKind::ArrayBegin;
    this->nextChar();
    return token;
  }
  case ']': {
    token.kind = TokenKind::ArrayEnd;
    this->nextChar();
    return token;
  }

    // Operators
  case '=': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Assign;
    c = this->nextChar();
    if (c == '=') {
      token._operator = Operator::EqualTo;
      this->nextChar();
    }
    return token;
  }
  case '+': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Add;
    this->nextChar();
    return token;
  }
  case '-': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Sub;
    this->nextChar();
    return token;
  }
  case '*': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Mul;
    this->nextChar();
    return token;
  }
  case '/': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Div;
    this->nextChar();
    return token;
  }
  case '%': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Mod;
    this->nextChar();
    return token;
  }
  case '|': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Bitwise_Or;
    c = this->nextChar();
    if (c == '|') {
      token._operator = Operator::Logical_Or;
      this->nextChar();
    }
    return token;
  }
  case '^': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Bitwise_Xor;
    this->nextChar();
    return token;
  }
  case '&': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Bitwise_And;
    c = this->nextChar();
    if (c == '&') {
      token._operator = Operator::Logical_And;
      this->nextChar();
    }
    return token;
  }
  case '<': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::LessThen;
    c = this->nextChar();
    if (c == '<') {
      token._operator = Operator::Bitwise_LeftShift;
      this->nextChar();
    } else if (c == '=') {
      token._operator = Operator::LessThenOrEqualTo;
      this->nextChar();
    }
    return token;
  }
  case '>': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::GreaterThen;
    c = this->nextChar();
    if (c == '>') {
      token._operator = Operator::Bitwise_RightShift;
      this->nextChar();
    } else if (c == '=') {
      token._operator = Operator::GreaterThenOrEqualTo;
      this->nextChar();
    }
    return token;
  }
  case '.': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::MemberAccess;
    this->nextChar();
    return token;
  }
  case '!': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Unary_Logical_Not;
    c = this->nextChar();
    if (c == '=') {
      token._operator = Operator::NotEqualTo;
      this->nextChar();
    }
    return token;
  }
  case '~': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Unary_Bitwise_Not;
    this->nextChar();
    return token;
  }
  }

  if (token.kind == TokenKind::Operator) {
    return token;
  }

  // Parse Name
  size_t start = this->index;
  while ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_') {
    c = this->nextChar();
  }

  token.kind = TokenKind::Name;
  token.text = this->source.range(start, this->index - 1);
  const std::string cpp_string((const char *)token.text.ptr, token.text.len);

  // Convert to Keyword
  if (keyword_mapping.count(cpp_string) != 0) {
    token.kind = TokenKind::Keyword;
    token.keyword = keyword_mapping.at(cpp_string);
    return token;
  }

  // Convert to Keyword Operator
  if (operator_keyword_mapping.count(cpp_string) != 0) {
    token.kind = TokenKind::Operator;
    token._operator = operator_keyword_mapping.at(cpp_string);
    return token;
  }

  return token;
}

char Tokenizer::nextChar() {
  if (this->source[this->index] == '\n') {
    this->line += 1;
    this->column = 1;
  } else {
    this->column += 1;
  }

  this->index += 1;
  if (this->index >= this->source.len) {
    return -1;
  }

  return this->source.ptr[this->index];
}
