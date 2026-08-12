#pragma once

#include "types.hpp"
#include <cstdint>

struct MIRLiteral; // Forward declaration
struct MIRBlock;   // Forward declaration

enum class MIRLiteralKind {
  Null,
  Typed,
};

struct SliceLiteral {
  size_t len;
  MIRLiteral *pointer;
};

struct MIRLiteral {
  Type *lit_type;
  MIRLiteralKind kind = MIRLiteralKind::Typed;
  union {
    bool _bool;
    int64_t _int;
    double _float;
    MIRLiteral *pointer;
    SliceLiteral slice;
    Type *_typeid;
    MIRBlock *function_entry;
    Slice<MIRLiteral> values;
  };
};
