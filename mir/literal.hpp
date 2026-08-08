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
  union {
    bool _bool;
    int64_t _int;
    double _float;
    MIRLiteral *pointer;
    SliceLiteral slice;
    Type *type;
    MIRBlock *function;
    Slice<MIRLiteral> values;
  };
};
