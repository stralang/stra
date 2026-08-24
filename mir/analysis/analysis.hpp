#pragma once

#include "../mir.hpp"
#include "allocator.hpp"
#include "comptime/comptime.hpp"
#include "containers.hpp"

struct MIRAnalyser {
  MIRComptime comptime_state;

  DynamicArena arena;
  Allocator *allocator;

  size_t error_count = 0;
  size_t warning_count = 0;
  void (*error_func)(SrcLoc srcloc, String msg);
  void (*warning_func)(SrcLoc srcloc, String msg);

  void init(Allocator *allocator);
  void analyse(MIRModule *module);
  void deinit();
};
