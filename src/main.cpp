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

namespace fs = std::filesystem;

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

  // Get Objects
  for (size_t i = 0; i < outputs.len; i++) {
    String output = outputs.ptr[i];
    std::string cpp_output((const char *)output.ptr, output.len);

    cmd.push_back(' ');
    cmd.append(cpp_output);
  }

  // Linker Scripts
  for (size_t i = 0; i < env->linker_scripts.length; i++) {
    String *path = env->linker_scripts.data.ptr + i;
    std::string script_path((const char *)path->ptr, path->len);
    cmd.append("-Wl,-T ");
    cmd.append(script_path);
  }

  // Link Directories
  for (size_t i = 0; i < env->link_directories.length; i++) {
    String *path = env->link_directories.data.ptr + i;
    std::string dir_path((const char *)path->ptr, path->len);

    cmd.append(" -Wl,-L");
    cmd.append(dir_path);
    cmd.append(" -Wl,-rpath,");

    if (dir_path[0] == '.') {
      cmd.append("'$ORIGIN");
      cmd.append(dir_path.substr(1));
      cmd.push_back('\'');
    } else {
      cmd.append(dir_path);
    }
  }

  // Link Libraries
  bool link_dynamic = true;
  for (size_t i = 0; i < env->link_libraries.length; i++) {
    Library *lib = env->link_libraries.data.ptr + i;
    std::string lib_name((const char *)lib->name.ptr, lib->name.len);

    // Enforce Type
    if (!link_dynamic && lib->scope == LibraryScope::Dynamic) {
      link_dynamic = true;
      cmd.append(" -Wl,-Bdynamic");
    } else if (link_dynamic && lib->scope == LibraryScope::Static) {
      link_dynamic = false;
      cmd.append(" -Wl,-Bstatic");
    }

    // Check Path vs Name
    if (lib_name[0] == '.' || lib_name[0] == '/') {
      cmd.push_back(' ');
    } else {
      cmd.append(" -l");
    }

    cmd.append(lib_name);
  }

  if (!link_dynamic) {
    cmd.append(" -Wl,-Bdynamic");
  }

  // Output
  std::string out_path((const char *)output_path.ptr, output_path.len);
  cmd.append(" -o ");
  cmd.append(out_path);

  // Flags
  for (size_t i = 0; i < env->linker_flags.length; i++) {
    String *s = env->linker_flags.data.ptr + i;
    std::string flags((const char *)s->ptr, s->len);

    cmd.append(" -Wl,");
    cmd.append(flags);
  }

  // Execute
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

struct SourceFile {
  String fullpath;
  uint64_t hashcode;

  Symbol *root;
  ASTParser parser;
};
struct SourceFiles {
  ArrayList<SourceFile> list;
  HashMap<uint64_t, size_t> indices;
  Allocator *allocator;

  void init(Allocator *allocator) {
    this->list.init(allocator, 32);
    this->indices.init(allocator, 32);
    this->allocator = allocator;
  }

  Symbol *push(String fullpath) {
    Hasher hasher;
    hasher.hash_raw(fullpath.ptr, fullpath.len);

    this->indices.insert(hasher.state, this->list.length);

    Symbol *symbol = (Symbol *)this->allocator->alloc(sizeof(Symbol));
    symbol->parent = nullptr;
    symbol->children.init(allocator, 8);

    this->list.push(SourceFile{
        .fullpath = fullpath,
        .hashcode = hasher.state,
        .root = symbol,
    });
    return symbol;
  }

  SourceFile *getPtrUnchecked(size_t i) { return this->list.data.ptr + i; }
  size_t *getIndex(uint64_t hashcode) { return this->indices.get(hashcode); }

  size_t len() { return this->list.length; }
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

  // Initialize
  TypeCache type_cache;
  type_cache.init(&global_allocator);

  // Compile
  SourceFiles files;
  HashMap<uint64_t, size_t> path_to_file;

  files.init(&global_allocator);
  path_to_file.init(&global_allocator, 64);

  // Add command line supplied files
  for (size_t i = 0; i < args.paths.length; i++) {
    String path = args.paths.data.ptr[i];

    std::string cpp_path((const char *)path.ptr, path.len);
    fs::path cpp_fullpath = fs::canonical(cpp_path);
    std::string cpp_fullpath_str = cpp_fullpath.string();
    String out = {
        cpp_fullpath_str.length(),
        global_allocator.alloc(cpp_fullpath_str.length() * sizeof(char)),
    };
    memcpy(out.ptr, cpp_fullpath_str.data(),
           cpp_fullpath_str.length() * sizeof(char));

    files.push(out);
  }

  size_t total_parse_errors = 0;
  size_t file_idx = 0;
  while (file_idx < files.len()) {
    SourceFile *file = files.getPtrUnchecked(file_idx);
    std::string cpp_fullpath_str((const char *)file->fullpath.ptr,
                                 file->fullpath.len);
    fs::path cpp_fullpath = cpp_fullpath_str;

    std::string cpp_filename = cpp_fullpath.stem();
    String filename = {
        cpp_filename.length(),
        global_allocator.alloc(cpp_filename.length() * sizeof(char))};
    memcpy(filename.ptr, cpp_filename.data(),
           cpp_filename.length() * sizeof(char));

    // Tokenize
    Tokenizer tokenizer;
    tokenizer.path = file->fullpath;
    tokenizer.path_hashcode = file->hashcode;
    tokenizer.init();

    // Parse
    // FIXME: `filename` Two files with the same name can causes linker errors
    file->parser = ASTParser{
        .tokenizer = tokenizer,
        .filename = filename,
        .symbol = file->root,
        .error_func = &error_handler,
        .type_cache = &type_cache,
        .allocator = &global_allocator,
    };
    file->parser.parse();

    // Emit AST
    if (args.emit_mode == EmitMode::AST) {
      std::cout << "---- " << file->fullpath << " ----\n";
      std::cout << *file->parser.ast << "\n";
    }

    total_parse_errors += file->parser.error_count;

    // Handle Imports
    for (size_t i = 0; i < file->parser.imports.length; i++) {
      Node *import = file->parser.imports.data.ptr[i];
      String import_path = import->import.path;

      std::string cpp_path((const char *)import_path.ptr, import_path.len);
      fs::path fullpath = fs::canonical(cpp_fullpath.parent_path() / cpp_path);
      std::string fullpath_str = fullpath.string();

      Hasher hasher;
      hasher.hash_raw((uint8_t *)fullpath_str.data(), fullpath_str.length());

      // Check existing
      size_t *import_idx = files.getIndex(hasher.state);
      if (import_idx != nullptr) {
        SourceFile *import_file = files.getPtrUnchecked(*import_idx);
        import->import.scope = import_file->root;
        continue;
      }

      // Add new file
      String out = {
          fullpath_str.length(),
          global_allocator.alloc(fullpath_str.length() * sizeof(char)),
      };
      memcpy(out.ptr, fullpath_str.data(),
             fullpath_str.length() * sizeof(char));
      import->import.scope = files.push(out);
    }

    file_idx += 1;
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
  environment.link_directories.init(&global_allocator, 8);
  environment.linker_scripts.init(&global_allocator, 4);
  environment.linker_flags.init(&global_allocator, 16);

  CodeGenContext codegen_ctx;
  codegen_ctx.init(&environment, args.target_triple);

  // Evaluate
  SourceFile *root_file = files.getPtrUnchecked(0);
  Evaluator evaluator = {
      .ast = root_file->root->node,
      .symbol = root_file->root,
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
    for (size_t i = 0; i < files.len(); i++) {
      SourceFile *file = files.getPtrUnchecked(i);
      std::cout << "---- " << file->fullpath << " ----\n";
      std::cout << *file->root->node << "\n";
    }

    return 0;
  }

  // Code Gen
  ArrayList<String> outputs;
  outputs.init(&global_allocator, 8);

  bool emit_ir = args.emit_mode == EmitMode::IR;
  bool emit_asm = args.emit_mode == EmitMode::Assembly;

  for (size_t i = 0; i < files.len(); i++) {
    SourceFile *file = files.getPtrUnchecked(i);

    // Get File name
    std::string cpp_fullpath_str((const char *)file->fullpath.ptr,
                                 file->fullpath.len);
    fs::path name_path = cpp_fullpath_str;
    name_path = name_path.filename();
    if (emit_ir) {
      name_path.replace_extension("ll");
    } else if (emit_asm) {
      name_path.replace_extension("S");
    } else {
      name_path.replace_extension("o");
    }

    std::string cpp_name = name_path.string();

    String out_name = {
        .len = cpp_name.size(),
        .ptr = (uint8_t *)global_allocator.alloc(cpp_name.size()),
    };
    memcpy(out_name.ptr, cpp_name.data(), cpp_name.size());
    outputs.push(out_name);

    // Get module name
    std::string cpp_module_name((const char *)file->parser.filename.ptr,
                                file->parser.filename.len);
    cpp_module_name.append("-");
    cpp_module_name.append(std::to_string(file->hashcode));
    String module_name = {cpp_module_name.length(),
                          (uint8_t *)cpp_module_name.data()};

    // Generate
    CodeGenModule codegen = {
        .module_name = module_name,
        .source_path_hashcode = file->hashcode,
        .ast = file->root->node,
        .symbol = file->root,
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
