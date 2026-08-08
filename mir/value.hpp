#pragma once

#include "types.hpp"
#include <cstdint>

enum class ValueKind {
  Null,
  Type,
  Bool,
  Integer,
  Float,
};

struct Value {
  ValueKind kind;
  union {
    Type *type;
    bool _bool;
    int64_t _int;
    double _float;
  };
};
