#include "literal.hpp"
#include "mir.hpp"
#include "mirgen.hpp"

MIRValueId valueToMIR(MIRGen *mirgen, Value *value) {
  MIRLiteral literal;
  switch (value->type->kind) {
  case TypeKind::Bool: {
    literal.lit_type = mirgen->ctx->type_cache->get({.kind = TypeKind::Bool});
    literal._bool = value->data._bool;
    break;
  }
  case TypeKind::Integer: {
    Type ty = {.kind = TypeKind::Integer};
    ty.integer = {.is_untyped = true};

    literal.lit_type = mirgen->ctx->type_cache->get(ty);
    literal._int = value->data.integer;
    break;
  }
  case TypeKind::Float: {
    Type ty = {.kind = TypeKind::Float};
    ty._float = {.is_untyped = true};

    literal.lit_type = mirgen->ctx->type_cache->get(ty);
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
    literal.lit_type = value->type;
    literal._int = value->data.integer;
    break;
  }
  }

  MIRValue out = {
      .kind = MIRValueKind::Literal,
      .literal = literal,
  };
  return mirgen->builder.insert(out);
}
