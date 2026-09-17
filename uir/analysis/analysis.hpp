#pragma once

#include "../comptime/comptime.hpp"
#include "../uir.hpp"
#include "allocator.hpp"
#include "containers.hpp"

struct UIRAnalyser {
  UIRContext *ctx;
  UIRComptime comptime_state;

  DynamicArena arena;
  Allocator *allocator;

  size_t error_count = 0;
  size_t warning_count = 0;
  void (*error_func)(SrcLoc srcloc, String msg);
  void (*warning_func)(SrcLoc srcloc, String msg);

  void init(Allocator *allocator);
  void analyse(UIRModule *module);
  void deinit();
};
