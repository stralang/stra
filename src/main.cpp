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
#include <iostream>
#include <string>

const char *NAME = "Stra";
const char *VERSION = "0";

enum class EmitMode {
  Executable,
  Object,
  IR,
  EvaluatedAST,
  AST,
};

struct Args {
  EmitMode emit_mode = EmitMode::Executable;
  bool run = false;
  ArrayList<String> paths;
  String output_path;
};

int main(int argc, const char **argv) {
  Allocator global_allocator;

  // Parse Arguments
  Args args;
  args.paths.init(&global_allocator, 4);
  args.output_path.ptr = nullptr;

  size_t i = 1;
  bool print_help = false;
  while (i < argc) {
    if (strcmp(argv[i], "--help") == 0) {
      print_help = true;
      break;
    } else if (strcmp(argv[i], "--version") == 0) {
      std::cout << NAME << " " << VERSION << "\n";
      return 0;
    } else if (strcmp(argv[i], "--run") == 0 || strcmp(argv[i], "-r") == 0) {
      args.run = true;
    } else if (strcmp(argv[i], "--emit") == 0 || strcmp(argv[i], "-e") == 0) {
      i += 1;
      if (argc <= i) {
        std::cerr << "emit flag missing mode\n";
        return 1;
      }

      if (strcmp(argv[i], "executable") == 0) {
        args.emit_mode = EmitMode::Executable;
      } else if (strcmp(argv[i], "object") == 0) {
        args.emit_mode = EmitMode::Object;
      } else if (strcmp(argv[i], "ir") == 0) {
        args.emit_mode = EmitMode::IR;
      } else if (strcmp(argv[i], "evaluated") == 0) {
        args.emit_mode = EmitMode::EvaluatedAST;
      } else if (strcmp(argv[i], "ast") == 0) {
        args.emit_mode = EmitMode::AST;
      }
    } else if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) {
      i += 1;
      if (argc <= i) {
        std::cerr << "out flag missing path\n";
        return 1;
      }

      args.output_path = {
          .len = strlen(argv[i]),
          .ptr = (uint8_t *)argv[i],
      };
    } else {
      args.paths.push({
          .len = strlen(argv[i]),
          .ptr = (uint8_t *)argv[i],
      });
    }

    i += 1;
  }

  if (print_help || args.paths.length == 0) {
    std::cout << "`stra <paths> [options]`\n";
    std::cout << "\nOptions:\n";
    std::cout << "  `--run` build and execute\n";
    std::cout << "  `--emit`\n";
    std::cout << "      `executable` Emit executable [default]\n";
    std::cout << "      `object`     Emit object files\n";
    std::cout << "      `ir`         Emit llvm ir files\n";
    std::cout << "      `ast`        Prints parsed ASTs\n";
    std::cout << "      `evaluated`  Prints evaluated ASTs\n";
    std::cout << "  `--output` output path\n";
    return 0;
  }

  // Compile
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

  ArrayList<SourceFile> files;
  ArrayList<PendingFile> pending;
  HashMap<uint64_t, size_t> path_to_file;

  files.init(&global_allocator, 8);
  pending.init(&global_allocator, 8);
  path_to_file.init(&global_allocator, 64);

  for (size_t i = 0; i < args.paths.length; i++) {
    pending.push({
        .path = args.paths.data.ptr[i],
        .importer = nullptr,
    });
  }

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
    if (args.emit_mode == EmitMode::AST) {
      std::cout << "---- " << file.path << " ----\n";
      std::cout << *parser.ast << "\n";
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

  if (args.emit_mode == EmitMode::AST) {
    return 0;
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
  if (args.emit_mode == EmitMode::EvaluatedAST) {
    for (size_t i = 0; i < files.length; i++) {
      SourceFile *file = files.data.ptr + i;
      std::cout << "---- " << file->path << " ----\n";
      std::cout << *file->ast << "\n";
    }

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
  if (args.emit_mode == EmitMode::IR) {
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

  if (args.emit_mode == EmitMode::Executable) {
    if (args.output_path.ptr != nullptr) {
      std::string out_path((const char *)args.output_path.ptr,
                           args.output_path.len);
      clang_cmd.append(" -o ");
      clang_cmd.append(out_path);
    }

    std::system(clang_cmd.data());
  } else if (args.emit_mode == EmitMode::Object) {
    clang_cmd.append(" -c");
    if (args.output_path.ptr != nullptr) {
      std::string out_path((const char *)args.output_path.ptr,
                           args.output_path.len);
      clang_cmd.append(" -o ");
      clang_cmd.append(out_path);
    }

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
