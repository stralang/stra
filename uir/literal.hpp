#pragma once

#include "optional.hpp"
#include "rir/rir.hpp"
#include "rir/type.hpp"
#include "types.hpp"
#include <cstdint>

// Forward declarations
struct UIRLiteral;
struct UIRRawData;
struct UIRValue;
// Forward declarations

enum class UIRPlaceKind {
  Inst,
  Raw,
};

struct UIRPlace {
  UIRPlaceKind kind;
  union {
    RIRValueId inst;
    UIRRawData *data;
  };
};

enum class UIRRawDataKind {
  Void,
  Bool,
  Int,
  Float,
  Pointer,
  Slice,
  TypeId,
  Namespace,
};

struct UIRRawData {
  UIRRawDataKind kind;
  union {
    bool _bool;
    int64_t _int;
    double _float;
    UIRPlace ptr;
    Slice<UIRRawData> values;
    RIRTypeId _typeid;
    UIRValue *_namespace;
  };
};

struct SliceLiteral {
  size_t len;
  UIRLiteral *pointer;
};

struct UIRLiteral {
  UIRRawData data;
  Option<RIRTypeId> lit_type = {};
};
