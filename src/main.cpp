#include "allocator.hpp"
#include "codegen.hpp"
#include "containers.hpp"
#include "evaluator.hpp"
#include "parser.hpp"
#include "tokenizer.hpp"
#include <cstdlib>
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

  TypeCache type_cache;
  Evaluator evaluator = {
      .ast = parser.ast,
      .scope = parser.scope,
      .type_cache = &type_cache,
      .allocator = &allocator,
  };
  evaluator.eval();

  CodeGenContext codegen_ctx;
  codegen_ctx.init();

  CodeGenModule codegen = {
      .source_path = tokenizer.path,
      .ast = parser.ast,
      .scope = parser.scope,
      .allocator = &allocator,
  };
  codegen.output_path = "out.bc";
  codegen.generate(&codegen_ctx);

  std::system("clang out.bc -o out");

  // std::cout << *parser.ast << "\n";
  // std::cout << *parser.scope << "\n";

  // while (true) {
  //   Token token = tokenizer.next();
  //   if (token.kind == TokenKind::Eof) {
  //     break;
  //   }
  //   std::cout << token << "\n";
  // }

  codegen_ctx.deinit();
  tokenizer.deinit();
}
