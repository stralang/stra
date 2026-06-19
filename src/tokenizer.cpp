#include "tokenizer.hpp"
#include "token.hpp"
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <iostream>
#include <map>
#include <string>

std::map<std::string, TokenKind> keyword_mapping = {
    {"fn", TokenKind::Function},
    {"struct", TokenKind::Struct},
    {"enum", TokenKind::Enum},
    {"union", TokenKind::Union},
    {"return", TokenKind::Return},
    {"if", TokenKind::If},
    {"else", TokenKind::Else},
    {"for", TokenKind::For},
    {"in", TokenKind::In},
    {"switch", TokenKind::Switch},
    {"break", TokenKind::Break},
    {"continue", TokenKind::Continue},
    {"defer", TokenKind::Defer},
    {"import", TokenKind::Import},
    {"comptime", TokenKind::Comptime},
    {"asm", TokenKind::Assembly},
    {"const", TokenKind::Const},
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

void numberFromString(Token *token, String slice, bool is_float,
                      size_t e_offset) {
  if (is_float) {
    double value = 0.0;
    auto [_ptr, _ec] = std::from_chars(
        (const char *)slice.ptr, (const char *)(slice.ptr + e_offset), value);

    // Scientific e notation
    if (e_offset < slice.len) {
      double multiplyer = 0.0;
      auto [_mul_ptr, _mul_ec] =
          std::from_chars((const char *)(slice.ptr + e_offset + 1),
                          (const char *)(slice.ptr + slice.len), value);
      value *= std::pow(10.0, multiplyer);
    }

    token->kind = TokenKind::Float;
    token->_float = value;
    return;
  }

  // Base
  size_t base = 10;
  size_t offset = 0;
  if (slice[0] == '0' && slice.len > 1) {
    char c = slice[1];
    if (c == 'b') {
      base = 2;
      offset = 2;
    } else if (c == 'o') {
      base = 8;
      offset = 2;
    } else if (c == 'x') {
      base = 16;
      offset = 2;
    }
  }

  // Convert
  int64_t value = 0;
  auto [_ptr, _ec] =
      std::from_chars((const char *)(slice.ptr + offset),
                      (const char *)(slice.ptr + e_offset), value, base);

  // Scientific e notation
  if (e_offset < slice.len) {
    int64_t multiplyer = 0;
    auto [_mul_ptr, _mul_ec] =
        std::from_chars((const char *)(slice.ptr + e_offset + 1),
                        (const char *)(slice.ptr + slice.len), multiplyer);
    value *= std::pow(10, multiplyer);
  }

  token->kind = TokenKind::Integer;
  token->integer = value;
}

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

  // Parse Literals
  {
    bool is_number = c >= '0' && c <= '9';
    bool is_negative = c == '-';
    if (is_negative && this->index + 1 < this->source.len) {
      char ch = this->source.ptr[this->index + 1];
      is_number = ch >= '0' && ch <= '9';
      if (is_number) {
        c = this->nextChar();
      }
    }

    if (is_number) {
      // Parse Integer/Float
      size_t start = this->index;
      if (is_negative) {
        start -= 1;
      }

      bool is_float = false;
      size_t e_offset = 0;
      while ((c >= '0' && c <= '9') || c == '.' || c == 'x' || c == 'o' ||
             c == 'b' || c == 'e') {
        if (c == '.') {
          if (is_float) {
            break;
          }

          // Skip if `.` is part of a range token
          if (this->index + 1 < this->source.len &&
              this->source[this->index + 1] == '.') {
            break;
          }

          is_float = true;
        } else if (c == 'e') {
          e_offset = this->index - start;
        }

        c = this->nextChar();
      }

      if (e_offset == 0) {
        e_offset = this->index - 1;
      }

      numberFromString(&token, this->source.range(start, this->index - 1),
                       is_float, e_offset);
      return token;
    }
  }

  if (c == '\'') {
    // Parse char
    char value = this->nextChar();
    if (this->nextChar() == '\'') {
      token.kind = TokenKind::Char;
      token.integer = value;
      this->nextChar();
    }
    return token;
  } else if (c == '"') {
    // Parse String
    c = this->nextChar(); // Skip `"`

    size_t start = this->index;
    while (c != '"') {
      c = this->nextChar();
    }

    token.kind = TokenKind::String;
    token.text = this->source.range(start, this->index - 1);
    this->nextChar();
    return token;
  }

  // Parse Symbols and Operators
  switch (c) {
    // Symbols
  case '$': {
    token.kind = TokenKind::Comptime;
    this->nextChar();
    return token;
  }
  case ':': {
    token.kind = TokenKind::TypeSeperator;
    this->nextChar();
    return token;
  }
  case '@': {
    token.kind = TokenKind::Attribute;
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
    token.kind = TokenKind::Eq;
    c = this->nextChar();
    if (c == '=') {
      token.kind = TokenKind::Operator;
      token._operator = Operator::EqualTo;
      this->nextChar();
    } else if (c == '>') {
      token.kind = TokenKind::Case;
      this->nextChar();
    }
    return token;
  }
  case '+': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Add;
    c = this->nextChar();
    if (c == '=') {
      token.kind = TokenKind::Assignment;
      this->nextChar();
    }
    return token;
  }
  case '-': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Sub;
    c = this->nextChar();
    if (c == '-' && this->index + 1 < this->source.len &&
        this->source.ptr[this->index + 1] == '-') {
      token.kind = TokenKind::Undefined;
      this->nextChar();
      this->nextChar();
    } else if (c == '=') {
      token.kind = TokenKind::Assignment;
      this->nextChar();
    }
    return token;
  }
  case '*': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Mul;
    c = this->nextChar();
    if (c == '=') {
      token.kind = TokenKind::Assignment;
      this->nextChar();
    }
    return token;
  }
  case '/': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Div;
    c = this->nextChar();
    if (c == '=') {
      token.kind = TokenKind::Assignment;
      this->nextChar();
    }
    return token;
  }
  case '%': {
    token.kind = TokenKind::Operator;
    token._operator = Operator::Mod;
    c = this->nextChar();
    if (c == '=') {
      token.kind = TokenKind::Assignment;
      this->nextChar();
    }
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
    c = this->nextChar();
    if (c == '.' && this->index + 1 < this->source.len) {
      char peek = this->source[this->index + 1];
      if (peek == '<') {
        token.kind = TokenKind::RangeLessThen;
        this->nextChar();
        this->nextChar();
      } else if (peek == '=') {
        token.kind = TokenKind::RangeEqualTo;
        this->nextChar();
        this->nextChar();
      }
    }
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
    token.kind = keyword_mapping.at(cpp_string);
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
