cmake_minimum_required(VERSION 3.30)

project(mir VERSION 0.1.0 LANGUAGES CXX)
add_library(mir STATIC lib.cpp)

target_include_directories(mir PUBLIC ./)

set_target_properties(mir PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)
