#include "uir/literal.hpp"
#include "uir/uir.hpp"
#include "uirgen.hpp"

UIRValue *valueToUIR(UIRGen *uirgen, Value *value) {
  UIRRawData raw_data;
  switch (value->type->kind) {
  case TypeKind::Bool: {
    raw_data.kind = UIRRawDataKind::Bool;
    raw_data._bool = value->data._bool;
    break;
  }
  case TypeKind::Integer: {
    raw_data.kind = UIRRawDataKind::Int;
    raw_data._int = value->data.integer;
    break;
  }
  case TypeKind::Float: {
    raw_data.kind = UIRRawDataKind::Float;
    raw_data._float = value->data._float;
    break;
  }
  }

  return uirgen->builder.buildLiteral({.data = raw_data});
}
