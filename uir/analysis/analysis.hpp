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

struct UIRExtraTypeInfo {
  UIRValue *creator; // the creator instruction, this is used to find methods
  Slice<UIRLiteral> constants; // Struct defaults, Enum members, etc
};

struct UIRAnalyser {
  // UIR
  UIRContext *ctx;
  UIRComptime comptime_state;

  // RIR
  RIRContext *rir_ctx;
  RIRBuilder builder;

  HashMap<UIRValueId, UIRResolved> resolved_mapping;
  HashMap<UIRBlockId, RIRBlockId> resolved_block_mapping;
  HashMap<RIRTypeId, UIRExtraTypeInfo> type_extras;
  HashMap<UIRValueId, String> mangled_name_cache;

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
