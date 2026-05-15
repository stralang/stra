#include "allocator.hpp"
#include "ast.hpp"
#include "evaluator.hpp"
#include "parser.hpp"
#include "print.hpp"
#include "tokenizer.hpp"
#include <iostream>
#include <string>

int main(int argc, const char **argv) {
  std::string path = "test.stra";

  if (argc == 2) {
    path = argv[1];
  }

  Tokenizer tokenizer;
  tokenizer.path = Slice<uint8_t>{
      .len = path.size(),
      .ptr = (uint8_t *)path.data(),
  };
  tokenizer.init();

  Allocator allocator;
  ASTParser parser = ASTParser{
      .tokenizer = tokenizer,
      .allocator = &allocator,
  };
  parser.parse();

  Evaluator evaluator = {
      .ast = parser.ast,
      .scope = parser.scope,
      .allocator = &allocator,
  };
  evaluator.eval();

  std::cout << *parser.ast << "\n";
  // std::cout << *parser.scope << "\n";

  // while (true) {
  //   Token token = tokenizer.next();
  //   if (token.kind == TokenKind::Eof) {
  //     break;
  //   }
  //   std::cout << token << "\n";
  // }

  tokenizer.deinit();
}
