#include "allocator.hpp"
#include "codegen/codegen.hpp"
#include "containers.hpp"
#include "environment.hpp"
#include "evaluator/evaluator.hpp"
#include "parser.hpp"
#include "print.hpp"
#include "token.hpp"
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
  Assembly,
  IR,
  EvaluatedAST,
  AST,
};

enum class Linker {
  Clang,
  LD,
};

void link(Linker linker, Slice<String> outputs, EmitMode emit,
          String output_path, Environment *env) {
  std::string cmd;

  switch (linker) {
  case Linker::Clang: {
    cmd = "clang";
    break;
  }
  case Linker::LD: {
    cmd = "ld";
    break;
  }
  }

  for (size_t i = 0; i < outputs.len; i++) {
    String output = outputs.ptr[i];
    std::string cpp_output((const char *)output.ptr, output.len);

    cmd.append(" ");
    cmd.append(cpp_output);
  }

  if (emit == EmitMode::Object) {
    switch (linker) {
    case Linker::Clang: {
      cmd.append(" -c");
      break;
    }
    case Linker::LD: {
      cmd.append("");
      break;
    }
    }
  }

  for (size_t i = 0; i < env->link_libraries.length; i++) {
    Library *lib = env->link_libraries.data.ptr + i;
    std::string lib_name((const char *)lib->name.ptr, lib->name.len);

    if (lib->scope == LibraryScope::Dynamic) {
      cmd.append(" -l");
    }

    cmd.append(lib_name);
  }

  std::string out_path((const char *)output_path.ptr, output_path.len);
  cmd.append(" -o ");
  cmd.append(out_path);

  std::system(cmd.data());
}

struct Args {
  EmitMode emit_mode = EmitMode::Executable;
  Linker linker = Linker::Clang;
  Optimization optimize = Optimization::Minimal;
  String target_triple = {.len = 0, .ptr = nullptr};

  bool run = false;
  ArrayList<String> paths;
  String output_path = {.len = 5, .ptr = (uint8_t *)"a.out"};
};

void error_handler(SrcLoc srcloc, String msg) {
  std::cerr << "\e[0;31mERROR: \e[0m" << srcloc << " " << msg << "\n";
}

void warning_handler(SrcLoc srcloc, String msg) {
  std::cerr << "\e[0;33mWARNING: \e[0m" << srcloc << " " << msg << "\n";
}

int main(int argc, const char **argv) {
  Allocator global_allocator;

  // Parse Arguments
  Args args;
  args.paths.init(&global_allocator, 4);

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
      } else if (strcmp(argv[i], "asm") == 0) {
        args.emit_mode = EmitMode::Assembly;
      } else if (strcmp(argv[i], "ir") == 0) {
        args.emit_mode = EmitMode::IR;
      } else if (strcmp(argv[i], "evaluated") == 0) {
        args.emit_mode = EmitMode::EvaluatedAST;
      } else if (strcmp(argv[i], "ast") == 0) {
        args.emit_mode = EmitMode::AST;
      }
    } else if (strcmp(argv[i], "--linker") == 0) {
      i += 1;
      if (argc <= i) {
        std::cerr << "linker flag missing linker\n";
        return 1;
      }

      if (strcmp(argv[i], "clang") == 0) {
        args.linker = Linker::Clang;
      } else if (strcmp(argv[i], "ld") == 0) {
        args.linker = Linker::LD;
      }
    } else if (strcmp(argv[i], "-o") == 0) {
      i += 1;
      if (argc <= i) {
        std::cerr << "`-o` flag missing level\n";
        return 1;
      }

      if (strcmp(argv[i], "none") == 0) {
        args.optimize = Optimization::None;
      } else if (strcmp(argv[i], "minimal") == 0) {
        args.optimize = Optimization::Minimal;
      } else {
        std::cerr << "Unknown optimization level `" << argv[i] << "`\n";
        return 1;
      }
    } else if (strcmp(argv[i], "--output") == 0) {
      i += 1;
      if (argc <= i) {
        std::cerr << "out flag missing path\n";
        return 1;
      }

      args.output_path = {
          .len = strlen(argv[i]),
          .ptr = (uint8_t *)argv[i],
      };
    } else if (strcmp(argv[i], "--target") == 0) {
      i += 1;
      if (argc <= i) {
        std::cerr << "target flag missing target triple\n";
        return 1;
      }

      args.target_triple = {
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
    std::cout << "      `asm`         Emit assembly files\n";
    std::cout << "      `ir`         Emit llvm ir files\n";
    std::cout << "      `ast`        Prints parsed ASTs\n";
    std::cout << "      `evaluated`  Prints evaluated ASTs\n";
    std::cout << "  `--linker`\n";
    std::cout << "      `clang` Uses clang for linking [default]\n";
    std::cout << "      `ld` Uses ld for linking\n";
    std::cout << "  `-o`\n";
    std::cout << "      `none` No optimizations\n";
    std::cout << "      `minimal` Minimal optimizations [default]\n";
    std::cout << "  `--output` output path [default: 'a.out']\n";
    std::cout << "  `--target` Sets the target to compile for\n";
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

  size_t total_parse_errors = 0;
  while (pending.length > 0) {
    PendingFile file = pending.pop();

    // Tokenize
    Tokenizer tokenizer;
    tokenizer.path = file.path;
    tokenizer.init();

    // Parse
    ASTParser parser = ASTParser{
        .tokenizer = tokenizer,
        .error_func = &error_handler,
        .allocator = &global_allocator,
    };
    parser.parse();

    // Emit AST
    if (args.emit_mode == EmitMode::AST) {
      std::cout << "---- " << file.path << " ----\n";
      std::cout << *parser.ast << "\n";
    }

    total_parse_errors += parser.error_count;

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

  if (total_parse_errors > 0) {
    std::cerr << total_parse_errors << " errors, exiting.";
    return 1;
  }

  if (args.emit_mode == EmitMode::AST) {
    return 0;
  }

  // Setup CodeGen Context, and Environment
  Environment environment;
  environment.link_libraries.init(&global_allocator, 32);

  CodeGenContext codegen_ctx;
  codegen_ctx.init(&environment, args.target_triple);

  // Evaluate
  SourceFile *root_file = files.data.ptr;
  TypeCache type_cache;
  Evaluator evaluator = {
      .ast = root_file->ast,
      .symbol = root_file->symbol,
      .type_cache = &type_cache,
      .environment = &environment,
      .error_func = &error_handler,
      .warning_func = &warning_handler,
      .allocator = &global_allocator,
  };
  evaluator.eval();

  if (evaluator.warning_count > 0) {
    std::cout << "\e[0;33m" << evaluator.warning_count << " warnings.\e[0m\n";
  }

  if (evaluator.error_count > 0) {
    std::cerr << "\e[0;31m" << evaluator.error_count
              << " errors, exiting.\e[0m\n";
    return 1;
  }

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
  ArrayList<String> outputs;
  outputs.init(&global_allocator, 8);

  bool emit_ir = args.emit_mode == EmitMode::IR;
  bool emit_asm = args.emit_mode == EmitMode::Assembly;

  for (size_t i = 0; i < files.length; i++) {
    SourceFile *file = files.data.ptr + i;

    // Get File name
    std::string cpp_str((const char *)file->path.ptr, file->path.len);
    std::filesystem::path path = cpp_str;
    std::filesystem::path name = path.filename();
    if (emit_ir) {
      name.replace_extension("ll");
    } else if (emit_asm) {
      name.replace_extension("S");
    } else {
      name.replace_extension("o");
    }

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
    codegen.generate(&codegen_ctx, emit_ir, emit_asm, args.optimize);
  }

  // Skip linking
  if (args.emit_mode != EmitMode::Executable) {
    return 0;
  }

  // Link
  link(args.linker, outputs.slice(), args.emit_mode, args.output_path,
       &environment);

  // Cleanup
  for (size_t i = 0; i < outputs.length; i++) {
    String output = outputs.data.ptr[i];
    std::string cpp_output((const char *)output.ptr, output.len);
    std::filesystem::remove(cpp_output);
  }

  std::cout << "Compilation Success\n";

  // Execute
  if (args.run) {
    std::filesystem::path exe_path = "./";
    std::string s((const char *)args.output_path.ptr, args.output_path.len);
    exe_path.append(s);
    int status = std::system(exe_path.c_str());
    return WEXITSTATUS(status);
  }
}
