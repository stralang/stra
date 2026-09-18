#pragma once

#include "containers.hpp"
#include <cstdint>

enum class RIRConstantKind : uint8_t {
  Null,
  Bool,
  Integer,
  Float,
  List,
};

struct RIRConstant {
  RIRConstantKind kind;
  union {
    bool _bool;
    int64_t integer;
    double _float;
    Slice<RIRConstant> constants;
  };
};
