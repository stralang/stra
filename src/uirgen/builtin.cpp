#include "define.hpp"
#include "rir/type.hpp"
#include "uir/literal.hpp"
#include "uir/uir.hpp"
#include "uirgen.hpp"
#include <charconv>

UIRValue *genBuiltin(UIRGen *uirgen, String name) {
  std::string str((const char *)name.ptr, name.len);

  Option<RIRTypeId> out_type_id;
  if (str.compare("void") == 0) {
    out_type_id = uirgen->ctx->types->push({.kind = RIRTypeKind::Void});
  } else if (str.compare("typeid") == 0) {
    out_type_id = uirgen->ctx->types->push({.kind = RIRTypeKind::TypeId});
  } else if (str.compare("bool") == 0) {
    out_type_id = uirgen->ctx->types->push({.kind = RIRTypeKind::Bool});
  } else if (str.compare("usize") == 0) {
    RIRType t = {.kind = RIRTypeKind::Integer};
    t.integer = {.is_untyped = false, .is_signed = false, .bits = -1};
    out_type_id = uirgen->ctx->types->push(t);
  } else if (str.compare("isize") == 0) {
    RIRType t = {.kind = RIRTypeKind::Integer};
    t.integer = {.is_untyped = false, .is_signed = true, .bits = -1};
    out_type_id = uirgen->ctx->types->push(t);
  } else if (name.len >= 2 &&
             (name[0] == 'u' || name[0] == 'i' || name[0] == 'f')) {
    // Integer and Float
    uint32_t bits = 0;
    auto [ptr, ec] = std::from_chars((const char *)(name.ptr + 1),
                                     (const char *)(name.ptr + name.len), bits);

    if (ec == std::errc{}) {
      RIRType t = {.kind = RIRTypeKind::Void};
      if (name.ptr[0] == 'u' || name.ptr[0] == 'i') {
        t.kind = RIRTypeKind::Integer;
        t.integer = {
            .is_untyped = false,
            .is_signed = name.ptr[0] == 'i',
            .bits = (int32_t)bits,
        };
      } else if (name.ptr[0] == 'f' &&
                 (bits == 16 || bits == 32 || bits == 64 || bits == 128)) {
        t.kind = RIRTypeKind::Float;
        t._float = {.is_untyped = false, .bits = bits};
      }

      if (t.kind != RIRTypeKind::Void) {
        out_type_id = uirgen->ctx->types->push(t);
      }
    }
  }

  UIRLiteral literal;
  if (out_type_id.isSome()) {
    literal.data = {.kind = UIRRawDataKind::TypeId,
                    ._typeid = out_type_id.get()};
  } else if (str.compare("true") == 0) {
    literal.lit_type = uirgen->ctx->types->push({.kind = RIRTypeKind::Bool});
    literal.data = {.kind = UIRRawDataKind::Bool, ._bool = true};
  } else if (str.compare("false") == 0) {
    literal.lit_type = uirgen->ctx->types->push({.kind = RIRTypeKind::Bool});
    literal.data = {.kind = UIRRawDataKind::Bool, ._bool = false};
  } else {
    return nullptr;
  }

  return uirgen->builder.buildLiteral(literal);
}
