#pragma once

#include "../mir.hpp"
#include "allocator.hpp"
#include "comptime/comptime.hpp"
#include "containers.hpp"

struct MIRAnalyser {
  MIRComptime comptime_state;

  DynamicArena arena;
  Allocator *allocator;

  void init(Allocator *allocator);
  void analyse(MIRModule *module);
  void deinit();
};
