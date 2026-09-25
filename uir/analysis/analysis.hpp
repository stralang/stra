#pragma once

#include "../comptime/comptime.hpp"
#include "../uir.hpp"
#include "allocator.hpp"
#include "containers.hpp"
#include "rir/builder.hpp"
#include "rir/constant.hpp"
#include "rir/rir.hpp"
#include "rir/type.hpp"
#include "uir/literal.hpp"

enum class UIRResolvedKind {
  Inst,
  Literal,
};

struct UIRResolved {
  UIRResolvedKind kind;
  union {
    RIRValueId inst;
    UIRLiteral literal;
  };
};

struct UIRAnalyser {
  // UIR
  UIRContext *ctx;
  UIRComptime comptime_state;

  // RIR
  RIRContext *rir_ctx;
  RIRBuilder builder;

  HashMap<UIRValue *, UIRResolved> resolved_mapping;
  HashMap<UIRBlock *, RIRBlockId> resolved_block_mapping;

  // Misc
  DynamicArena arena;
  Allocator *allocator;

  size_t error_count = 0;
  size_t warning_count = 0;
  void (*error_func)(SrcLoc srcloc, String msg);
  void (*warning_func)(SrcLoc srcloc, String msg);

  void init(Allocator *allocator);
  RIRContext *analyse();
  void deinit();
};
