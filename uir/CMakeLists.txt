cmake_minimum_required(VERSION 3.30)

project(uir VERSION 0.1.0 LANGUAGES CXX)
add_library(uir STATIC
  uir.cpp
  builder.cpp

  analysis/analysis.cpp
  analysis/operator.cpp
  analysis/lookup.cpp
  analysis/global.cpp
  analysis/aggregate.cpp
  analysis/types.cpp

  comptime/comptime.cpp
  comptime/operator.cpp
  #comptime/global.cpp
)

target_include_directories(uir PUBLIC ./)

set_target_properties(uir PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_link_libraries(uir PRIVATE common)
target_link_libraries(uir PRIVATE rir)
