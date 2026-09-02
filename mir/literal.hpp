#pragma once

#include "types.hpp"
#include <cstdint>

struct MIRLiteral; // Forward declaration
struct MIRValue;   // Forward declaration

enum class MIRLiteralKind {
  Null,
  Typed,
  Instruction, // Only allowed for Pointer Literals
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
    Slice<MIRLiteral> values;
    MIRValue *instruction;
    uint8_t *inline_data;
  };
};
