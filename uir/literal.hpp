#pragma once

#include "types.hpp"
#include <cstdint>

struct UIRLiteral; // Forward declaration
struct UIRValue;   // Forward declaration

enum class UIRLiteralKind {
  Null,
  Typed,
  Instruction, // Only allowed for Pointer Literals
};

struct SliceLiteral {
  size_t len;
  UIRLiteral *pointer;
};

struct UIRLiteral {
  Type *lit_type;
  UIRLiteralKind kind = UIRLiteralKind::Typed;
  union {
    bool _bool;
    int64_t _int;
    double _float;
    UIRLiteral *pointer;
    SliceLiteral slice;
    Type *_typeid;
    Slice<UIRLiteral> values;
    UIRValue *instruction;
    uint8_t *inline_data;
  };
};
