#include "define.hpp"
#include "uir/literal.hpp"
#include "uir/uir.hpp"
#include "uirgen.hpp"
#include <charconv>

UIRValue *genBuiltin(UIRGen *uirgen, String name) {
  std::string str((const char *)name.ptr, name.len);

  Type *out_type = nullptr;
  if (str.compare("void") == 0) {
    out_type = uirgen->ctx->type_cache->get({.kind = TypeKind::Void});
  } else if (str.compare("typeid") == 0) {
    out_type = uirgen->ctx->type_cache->get({.kind = TypeKind::TypeId});
  } else if (str.compare("bool") == 0) {
    out_type = uirgen->ctx->type_cache->get({.kind = TypeKind::Bool});
  } else if (str.compare("usize") == 0) {
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = false, .is_signed = false, .bits = -1};
    out_type = uirgen->ctx->type_cache->get(t);
  } else if (str.compare("isize") == 0) {
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = false, .is_signed = true, .bits = -1};
    out_type = uirgen->ctx->type_cache->get(t);
  } else if (name.len >= 2 &&
             (name[0] == 'u' || name[0] == 'i' || name[0] == 'f')) {
    // Integer and Float
    uint32_t bits = 0;
    auto [ptr, ec] = std::from_chars((const char *)(name.ptr + 1),
                                     (const char *)(name.ptr + name.len), bits);

    if (ec == std::errc{}) {
      Type t = {.kind = TypeKind::Void};
      if (name.ptr[0] == 'u' || name.ptr[0] == 'i') {
        t.kind = TypeKind::Integer;
        t.integer = {
            .is_untyped = false,
            .is_signed = name.ptr[0] == 'i',
            .bits = (int32_t)bits,
        };
      } else if (name.ptr[0] == 'f' &&
                 (bits == 16 || bits == 32 || bits == 64 || bits == 128)) {
        t.kind = TypeKind::Float;
        t._float = {.is_untyped = false, .bits = bits};
      }

      if (t.kind != TypeKind::Void) {
        out_type = uirgen->ctx->type_cache->get(t);
      }
    }
  }

  UIRLiteral literal;
  if (out_type != nullptr) {
    literal.lit_type = uirgen->ctx->type_cache->get({.kind = TypeKind::TypeId});
    literal.kind = UIRLiteralKind::Typed;
    literal._typeid = out_type;
  } else if (str.compare("true") == 0) {
    literal.lit_type = uirgen->ctx->type_cache->get({.kind = TypeKind::Bool});
    literal.kind = UIRLiteralKind::Typed;
    literal._bool = true;
  } else if (str.compare("false") == 0) {
    literal.lit_type = uirgen->ctx->type_cache->get({.kind = TypeKind::Bool});
    literal.kind = UIRLiteralKind::Typed;
    literal._bool = false;
  } else {
    return nullptr;
  }

  return uirgen->builder.buildLiteral(literal);
}
