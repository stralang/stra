#include "allocator.hpp"
#include "codegen.hpp"
#include "containers.hpp"
#include "evaluator.hpp"
#include "parser.hpp"
#include "tokenizer.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

enum class EmitMode {
  Executable,
  Object,
  IR,
  EvaluatedAST,
  AST,
};

struct Args {
  bool run = false;
  EmitMode mode = EmitMode::Executable;
  String path;
};

int main(int argc, const char **argv) {
  Args args;
  args.path = "test.stra";

  // Parse Arguments
  size_t i = 1;
  while (i < argc) {
    if (strcmp(argv[i], "run") == 0) {
      args.run = true;
    } else if (strcmp(argv[i], "--emit") == 0) {
      i += 1;
      if (argc <= i) {
        std::cerr << "emit flag missing mode\n";
        return 1;
      }

      if (strcmp(argv[i], "executable") == 0) {
        args.mode = EmitMode::Executable;
      } else if (strcmp(argv[i], "object") == 0) {
        args.mode = EmitMode::Object;
      } else if (strcmp(argv[i], "ir") == 0) {
        args.mode = EmitMode::IR;
      } else if (strcmp(argv[i], "evaluated") == 0) {
        args.mode = EmitMode::EvaluatedAST;
      } else if (strcmp(argv[i], "ast") == 0) {
        args.mode = EmitMode::AST;
      }
    } else {
      args.path = argv[i];
    }

    i += 1;
  }

  // Tokenize
  Tokenizer tokenizer;
  tokenizer.path = args.path;
  tokenizer.init();

  // Parse
  Allocator allocator;
  ASTParser parser = ASTParser{
      .tokenizer = tokenizer,
      .allocator = &allocator,
  };
  parser.parse();

  // Emit AST
  if (args.mode == EmitMode::AST) {
    std::cout << parser.ast << "\n";
    return 0;
  }

  // Evaluate
  TypeCache type_cache;
  Evaluator evaluator = {
      .ast = parser.ast,
      .scope = parser.scope,
      .type_cache = &type_cache,
      .allocator = &allocator,
  };
  evaluator.eval();

  // Emit Evaluted AST
  if (args.mode == EmitMode::EvaluatedAST) {
    std::cout << parser.ast << "\n";
    return 0;
  }

  // Code Gen
  CodeGenContext codegen_ctx;
  codegen_ctx.init();

  CodeGenModule codegen = {
      .source_path = tokenizer.path,
      .ast = parser.ast,
      .scope = parser.scope,
      .allocator = &allocator,
  };
  codegen.output_path = "out.ll";
  codegen.generate(&codegen_ctx);

  // Emit Evaluted IR
  if (args.mode == EmitMode::IR) {
    return 0;
  }

  // Link
  if (args.mode == EmitMode::Executable) {
    std::system("clang out.ll -o out");
  } else if (args.mode == EmitMode::Object) {
    std::system("clang -c out.ll -o out.o");
  }
  std::filesystem::remove("out.ll");

  std::cout << "Compilation Success\n";

  // Execute
  if (args.run) {
    int status = std::system("./out");
    return WEXITSTATUS(status);
  }
}
