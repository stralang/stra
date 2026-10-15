#pragma once

#include "common/containers.hpp"
#include "common/inline.hpp"

struct UIRAttributes {
  Inline<String> link_name = {};
  Inline<String> builtin = {};
  bool implied_comptime = false; // Implies a comptime context
};
