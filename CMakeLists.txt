cmake_minimum_required(VERSION 3.30)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(stra)
add_executable(stra
  src/print.cpp
  src/tokenizer.cpp
  src/operator.cpp
  src/parser.cpp

  src/evaluator/evaluator.cpp
  src/evaluator/operator.cpp
  src/evaluator/function.cpp
  src/evaluator/type.cpp
  src/evaluator/builtin.cpp
  src/evaluator/desugar.cpp

  src/evaluator/comptime/comptime.cpp

  src/codegen/codegen.cpp
  src/codegen/convert.cpp
  src/codegen/operator.cpp
  src/codegen/function.cpp

  src/codegen/abi/general.cpp
  src/codegen/abi/sysvamd64.cpp

  src/environment.cpp

  src/main.cpp
)

# Get LLVM
find_program(LLVM_CONFIG llvm-config)
if(NOT LLVM_CONFIG)
  message(FATAL_ERROR, "llvm-config not found")
endif()

execute_process(
  COMMAND ${LLVM_CONFIG} --version
  OUTPUT_VARIABLE LLVM_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND ${LLVM_CONFIG} --includedir
  OUTPUT_VARIABLE LLVM_INCLUDE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND ${LLVM_CONFIG} --libdir
  OUTPUT_VARIABLE LLVM_LIB_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND ${LLVM_CONFIG} --libs
  OUTPUT_VARIABLE LLVM_LIBS
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

include_directories(${LLVM_INCLUDE_DIR})
target_link_libraries(stra ${LLVM_LIBS})
