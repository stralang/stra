#include "ast.hpp"
#include "print.hpp"
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

  ASTParser parser = ASTParser{
      .tokenizer = tokenizer,
  };
  parser.parse();

  std::cout << (int)parser.ast->kind << "\n";

  // while (true) {
  //   Token token = tokenizer.next();
  //   if (token.kind == TokenKind::Eof) {
  //     break;
  //   }
  //   std::cout << token << "\n";
  // }

  tokenizer.deinit();
}
