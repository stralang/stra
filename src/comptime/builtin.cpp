#include "comptime.hpp"

Value execBuiltinCall(InteropState *state, Node *node, Symbol *scope,
                      String name) {
  Value out;

  if (name.compare("sizeof")) {
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = false, .is_signed = false, .bits = -1};

    out.type = state->evaluator->type_cache->get(t);
    out.has_data = true;

    Node *arg_0 = node->call.arguments.data.ptr[0];
    out.data.integer = arg_0->value.data.type_value->sizeBits(
        state->evaluator->environment->pointer_size);
    out.data.integer = (out.data.integer + 7) / 8;
  } else if (name.compare("alignof")) {
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = false, .is_signed = false, .bits = -1};

    out.type = state->evaluator->type_cache->get(t);
    out.has_data = true;

    Node *arg_0 = node->call.arguments.data.ptr[0];
    out.data.integer = arg_0->value.data.type_value->alignBits(
        state->evaluator->environment->pointer_size);
    out.data.integer = (out.data.integer + 7) / 8;
  } else if (name.compare("linkLibrary")) {
    Node *name_arg = node->call.arguments.data.ptr[0];
    Node *scope_arg = node->call.arguments.data.ptr[1];

    state->evaluator->environment->link_libraries.push({
        .name = name_arg->value.data.text,
        .scope = (LibraryScope)scope_arg->value.data.integer,
    });

    out.type = state->evaluator->type_cache->get({.kind = TypeKind::Void});
    out.has_data = false;
  } else if (name.compare("linkDirectory")) {
    Node *path_arg = node->call.arguments.data.ptr[0];
    state->evaluator->environment->link_directories.push(
        path_arg->value.data.text);

    out.type = state->evaluator->type_cache->get({.kind = TypeKind::Void});
    out.has_data = false;
  } else if (name.compare("linkerScript")) {
    Node *path_arg = node->call.arguments.data.ptr[0];
    state->evaluator->environment->linker_scripts.push(
        path_arg->value.data.text);

    out.type = state->evaluator->type_cache->get({.kind = TypeKind::Void});
    out.has_data = false;
  } else if (name.compare("linkerFlags")) {
    Node *flags_arg = node->call.arguments.data.ptr[0];
    state->evaluator->environment->linker_flags.push(
        flags_arg->value.data.text);

    out.type = state->evaluator->type_cache->get({.kind = TypeKind::Void});
    out.has_data = false;
  }

  return out;
}
