#pragma once

#include "types.hpp"
#include <cstdint>

struct MIRLiteral; // Forward declaration
struct MIRBlock;   // Forward declaration

enum class MIRLiteralKind {
  Null,
  Bool,
  Integer,
  Float,
  Pointer,
  Slice,
  SIMD,
  TypeId,
  Function,
  Struct,
  Enum,
  Union
};

struct SliceLiteral {
  size_t len;
  MIRLiteral *pointer;
};

struct MIRLiteral {
  MIRLiteralKind kind;
  Type *lit_type;
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
