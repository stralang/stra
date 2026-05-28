#include "allocator.hpp"
#include "codegen.hpp"
#include "containers.hpp"
#include "evaluator.hpp"
#include "parser.hpp"
#include "print.hpp"
#include "tokenizer.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

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

  struct SourceFile {
    String path;
    Node *ast;
    Symbol *symbol;
    ASTParser parser;
  };
  struct PendingFile {
    String path;
    Node *importer;
  };

  Allocator global_allocator;
  ArrayList<SourceFile> files;
  ArrayList<PendingFile> pending;
  HashMap<uint64_t, size_t> path_to_file;

  files.init(&global_allocator, 8);
  pending.init(&global_allocator, 8);
  path_to_file.init(&global_allocator, 64);

  pending.push({
      .path = args.path,
      .importer = nullptr,
  });

  while (pending.length > 0) {
    PendingFile file = pending.pop();

    // Tokenize
    Tokenizer tokenizer;
    tokenizer.path = file.path;
    tokenizer.init();

    // Parse
    ASTParser parser = ASTParser{
        .tokenizer = tokenizer,
        .allocator = &global_allocator,
    };
    parser.parse();

    // Emit AST
    if (args.mode == EmitMode::AST) {
      std::cout << *parser.ast << "\n";
      return 0;
    }

    // Finish
    files.push({
        .path = file.path,
        .ast = parser.ast,
        .symbol = parser.symbol,
        .parser = parser,
    });

    Hasher hasher;
    hasher.hash_raw(file.path.ptr, file.path.len);
    path_to_file.insert(hasher.state, files.length - 1);

    if (file.importer != nullptr) {
      file.importer->import.node = parser.ast;
      file.importer->import.scope = parser.symbol;
    }

    // Handle Imports
    for (size_t i = 0; i < parser.imports.length; i++) {
      Node *import = parser.imports.data.ptr[i];
      String import_path = import->import.path;

      Hasher hasher;
      hasher.hash_raw(import_path.ptr, import_path.len);
      size_t *import_idx = path_to_file.get(hasher.state);
      if (import_idx != nullptr) {
        SourceFile *import_file = files.data.ptr + *import_idx;
        import->import.node = import_file->ast;
        import->import.scope = import_file->symbol;
      } else {
        pending.push({.path = import_path, .importer = import});
      }
    }
  }

  // Evaluate
  SourceFile *root_file = files.data.ptr;
  TypeCache type_cache;
  Evaluator evaluator = {
      .ast = root_file->ast,
      .symbol = root_file->symbol,
      .type_cache = &type_cache,
      .allocator = &global_allocator,
  };
  evaluator.eval();

  // Emit Evaluted AST
  if (args.mode == EmitMode::EvaluatedAST) {
    std::cout << *root_file->ast << "\n";
    return 0;
  }

  // Code Gen
  CodeGenContext codegen_ctx;
  codegen_ctx.init();

  ArrayList<String> outputs;
  outputs.init(&global_allocator, 8);

  for (size_t i = 0; i < files.length; i++) {
    SourceFile *file = files.data.ptr + i;

    // Get File name
    std::string cpp_str((const char *)file->path.ptr, file->path.len);
    std::filesystem::path path = cpp_str;
    std::filesystem::path name = path.filename();
    name.replace_extension(".ll");

    std::string cpp_name = name.string();

    String out_name = {
        .len = cpp_name.size(),
        .ptr = (uint8_t *)global_allocator.alloc(cpp_name.size()),
    };
    memcpy(out_name.ptr, cpp_name.data(), cpp_name.size());
    outputs.push(out_name);

    // Generate
    CodeGenModule codegen = {
        .source_path = file->path,
        .ast = file->ast,
        .symbol = file->symbol,
        .allocator = &global_allocator,
    };
    codegen.output_path = out_name;
    codegen.generate(&codegen_ctx);
  }

  // Emit Evaluted IR
  if (args.mode == EmitMode::IR) {
    return 0;
  }

  // Link
  std::string clang_cmd = "clang";
  for (size_t i = 0; i < outputs.length; i++) {
    String output = outputs.data.ptr[i];
    std::string cpp_output((const char *)output.ptr, output.len);

    clang_cmd.append(" ");
    clang_cmd.append(cpp_output);
  }

  if (args.mode == EmitMode::Executable) {
    clang_cmd.append(" -o out");
    std::cout << clang_cmd << "\n";
    std::system(clang_cmd.data());
  } else if (args.mode == EmitMode::Object) {
    clang_cmd.append(" -c -o out.o");
    std::system(clang_cmd.data());
  }

  // Cleanup
  for (size_t i = 0; i < outputs.length; i++) {
    String output = outputs.data.ptr[i];
    std::string cpp_output((const char *)output.ptr, output.len);
    std::filesystem::remove(cpp_output);
  }

  std::cout << "Compilation Success\n";

  // Execute
  if (args.run) {
    int status = std::system("./out");
    return WEXITSTATUS(status);
  }
}
