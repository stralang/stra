#pragma once

#include "containers.hpp"
#include <cstdint>

using RIRTypeId = uint32_t;

enum class RIRTypeKind : uint8_t {
  Void,
  Bool,
  Integer,
  Float,
  Pointer,
  Slice,
  Function,
  Struct,
  Enum,
  Union,
};

struct RIRType {
  RIRTypeKind kind;
  union {
    struct {
      bool is_signed;
      int32_t bits; // negative is pointer size
    } integer;
    uint32_t float_bits;
    RIRTypeId child;
    struct {
      int64_t length;
      RIRTypeId child;
    } slice;
    struct {
      Slice<RIRTypeId> arguments;
      RIRTypeId _return;
    } function;
    struct {
      Slice<RIRTypeId> fields;
    } _struct;
    struct {
      RIRTypeId repr;
    } _enum;
    struct {
      RIRTypeId repr;
      Slice<RIRTypeId> variants;
    } _union;
  };
};
