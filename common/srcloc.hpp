#pragma once

#include "containers.hpp"
#include <cstdint>

// A location in the human-readable source code
struct SrcLoc {
  String file;
  uint64_t file_hashcode;
  size_t index;
  size_t line;
  size_t column;
};
