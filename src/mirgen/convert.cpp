#include "literal.hpp"
#include "mir.hpp"
#include "mirgen.hpp"

MIRValue *valueToMIR(MIRGen *mirgen, Value *value) {
  MIRLiteral literal;
  switch (value->type->kind) {
  case TypeKind::Bool: {
    literal.kind = MIRLiteralKind::Bool;
    literal._bool = value->data._bool;
    break;
  }
  case TypeKind::Integer: {
    literal.kind = MIRLiteralKind::Integer;
    literal._int = value->data.integer;
    break;
  }
  case TypeKind::Float: {
    literal.kind = MIRLiteralKind::Float;
    literal._float = value->data._float;
    break;
  }
  case TypeKind::Slice: {
    // TODO:
    // literal.kind = MIRLiteralKind::Slice;
    // literal.slice = {.len = value->data.text.len, .pointer = };
    break;
  }
  case TypeKind::Enum: {
    literal.kind = MIRLiteralKind::Integer;
    literal._int = value->data.integer;
    break;
  }
  }

  return mirgen->ctx->makeLiteral(literal);
}
