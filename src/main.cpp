#include "tokenizer.hpp"
#include <iostream>
#include <string>

int main() {
  std::string path = "test.stra";

  Tokenizer tokenizer;
  tokenizer.path = Slice<uint8_t>{
      .len = path.size(),
      .ptr = (uint8_t *)path.data(),
  };
  tokenizer.init();

  while (true) {
    Token token = tokenizer.next();
    if (token.kind == TokenKind::Eof) {
      break;
    }
    std::cout << "[" << token.location.line << ":" << token.location.column
              << "] " << token.kind;
    switch (token.kind) {
    case TokenKind::Eof: {
      break;
    }
    case TokenKind::Comment:
    case TokenKind::Name: {
      std::cout << "`";
      std::cout.write((const char *)token.text.ptr, token.text.len);
      std::cout << "`";
      break;
    }
    }
    std::cout << "\n";
  }

  tokenizer.deinit();
}
