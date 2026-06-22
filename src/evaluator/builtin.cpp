#include "../print.hpp"
#include "define.hpp"
#include "evaluator.hpp"
#include <charconv>

Value getBuiltinValue(TypeCache *type_cache, String name) {
  std::string str((const char *)name.ptr, name.len);

  Type *out_type = nullptr;
  if (str.compare("void") == 0) {
    out_type = type_cache->get({.kind = TypeKind::Void});
  } else if (str.compare("typeid") == 0) {
    out_type = type_cache->get({.kind = TypeKind::TypeId});
  } else if (str.compare("bool") == 0) {
    out_type = type_cache->get({.kind = TypeKind::Bool});
  } else if (str.compare("usize") == 0) {
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = false, .is_signed = false, .bits = -1};
    out_type = type_cache->get(t);
  } else if (str.compare("isize") == 0) {
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = false, .is_signed = true, .bits = -1};
    out_type = type_cache->get(t);
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
        out_type = type_cache->get(t);
      }
    }
  }

  Value value = {.type = nullptr, .has_data = false};
  if (out_type != nullptr) {
    value.type = type_cache->get({.kind = TypeKind::TypeId});
    value.has_data = true;
    value.data.type_value = out_type;
  } else if (str.compare("true") == 0) {
    value.type = type_cache->get({.kind = TypeKind::Bool});
    value.has_data = true;
    value.data._bool = true;
  } else if (str.compare("false") == 0) {
    value.type = type_cache->get({.kind = TypeKind::Bool});
    value.has_data = true;
    value.data._bool = false;
  }

  return value;
}

void populateBuiltinVariable(Evaluator *evaluator, Node *node, Symbol *scope,
                             String name) {
  if (name.compare("TARGET_ARCH")) {
    node->value.data.integer = (uint64_t)evaluator->environment->target.arch;
  } else if (name.compare("TARGET_OS")) {
    node->value.data.integer = (uint64_t)evaluator->environment->target.os;
  } else if (name.compare("TARGET_SUB_OS")) {
    node->value.data.integer = (uint64_t)evaluator->environment->target.sub_os;
  } else if (name.compare("TARGET_VENDOR")) {
    node->value.data.text = evaluator->environment->target.vendor;
  } else if (name.compare("TARGET_ENDIAN")) {
    node->value.data.integer = (uint64_t)evaluator->environment->endianness;
  } else {
    expect(false, node->location,
           "Builtin variable `" << name << "` doesn't exist\n");
    return;
  }

  node->value.has_data = true;
  node->field.undefined = false;
}
