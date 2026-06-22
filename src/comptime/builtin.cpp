#include "comptime.hpp"

Value executeBuiltinCall(Evaluator *evaluator, Node *node, Symbol *scope,
                         String name) {
  Value out;

  if (name.compare("sizeof")) {
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = false, .is_signed = false, .bits = -1};

    out.type = evaluator->type_cache->get(t);
    out.has_data = true;

    Node *arg_0 = node->call.arguments.data.ptr[0];
    out.data.integer =
        arg_0->value.data.type_value->sizeBits(64); // TODO: Pointer size
    out.data.integer = (out.data.integer + 7) / 8;
  } else if (name.compare("alignof")) {
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = false, .is_signed = false, .bits = -1};

    out.type = evaluator->type_cache->get(t);
    out.has_data = true;

    Node *arg_0 = node->call.arguments.data.ptr[0];
    out.data.integer =
        arg_0->value.data.type_value->alignBits(64); // TODO: Pointer size
    out.data.integer = (out.data.integer + 7) / 8;
  }

  return out;
}
