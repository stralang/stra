cmake_minimum_required(VERSION 3.30)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(stra)
add_executable(stra
  src/print.cpp
  src/tokenizer.cpp
  src/operator.cpp
  src/parser.cpp
  src/main.cpp
)
