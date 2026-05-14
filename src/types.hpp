#pragma once

#include <cassert>
#include <cstdint>

struct Type;

struct IntegerType {
  bool is_untyped;
  bool is_signed;
  // -1 for native
  int32_t bits;
};

struct FloatType {
  bool is_untyped;
  uint32_t bits;
};

struct SliceType {
  // <0 for pointer slice, =0 for slice, >0 for array (comptile-time length)
  int64_t length;
  Type *type;
};

enum class TypeKind {
  Void,
  Bool,
  Integer,
  Float,
  Pointer,
  Slice,
  SIMD,
  Constant,
  TypeId,
};

struct Type {
  TypeKind kind;
  union {
    Type *child;
    IntegerType integer;
    FloatType _float;
    SliceType slice;
  };
};
