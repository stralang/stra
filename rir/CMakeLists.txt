cmake_minimum_required(VERSION 3.30)

project(rir VERSION 0.1.0 LANGUAGES CXX)
add_library(rir STATIC
  rir.cpp
  builder.cpp
)

target_include_directories(rir PUBLIC ./)

set_target_properties(rir PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_link_libraries(rir PRIVATE common)
