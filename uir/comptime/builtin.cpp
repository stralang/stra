#include "../analysis/analysis.hpp"
#include "define.hpp"
#include "uir/comptime/comptime.hpp"
#include "uir/environment.hpp"
#include "uir/literal.hpp"
#include "uir/uir.hpp"
#include <cmath>

void executeBuiltinCall(UIRComptime *state, UIRValue *func_inst) {
  String name = func_inst->name;
  if (func_inst->function.attributes.builtin.isInline()) {
    name = func_inst->function.attributes.builtin.get();
  } else if (func_inst->function.attributes.link_name.isInline()) {
    name = func_inst->function.attributes.link_name.get();
  }

  ComptimeStackFrame *frame = state->currentStack();
  if (name.compare("sizeof")) {
    assert(frame->values.length == 1);

    // Get Arguments
    UIRLiteral *lit = *frame->values.back();
    assert(lit->data.kind == UIRRawDataKind::TypeId);

    // Get Size of Type
    RIRType *ty = state->ctx->getType(lit->data._typeid);
    size_t size_bits = ty->sizeBits(state->ctx->types,
                                    state->analyser->environment->pointer_size);
    int64_t size = std::ceil((double)size_bits / 8.0);

    // Finish
    frame->inject({.data = {.kind = UIRRawDataKind::Int, ._int = size}});
  } else if (name.compare("alignof")) {
    assert(frame->values.length == 1);

    // Get Arguments
    UIRLiteral *lit = *frame->values.back();
    assert(lit->data.kind == UIRRawDataKind::TypeId);

    // Get Alignment of Type
    RIRType *ty = state->ctx->getType(lit->data._typeid);
    size_t align_bits = ty->alignBits(
        state->ctx->types, state->analyser->environment->pointer_size);
    int64_t align = std::ceil((double)align_bits / 8.0);

    // Finish
    frame->inject({.data = {.kind = UIRRawDataKind::Int, ._int = align}});
  } else if (name.compare("linkLibrary")) {
    assert(frame->values.length == 2);

    // Get Arguments
    UIRLiteral *name_arg =
        *frame->values.getPtrUnchecked(frame->values.length - 2);
    UIRLiteral *scope_arg =
        *frame->values.getPtrUnchecked(frame->values.length - 1);
    assert(name_arg->data.kind == UIRRawDataKind::Slice);
    assert(scope_arg->data.kind == UIRRawDataKind::Int);

    // Values to String
    String out_name = {
        .ptr = state->allocator->alloc(name_arg->data.values.len),
        .len = name_arg->data.values.len,
    };
    for (size_t i = 0; i < name_arg->data.values.len; i++) {
      UIRRawData data = name_arg->data.values[i];
      assert(data.kind == UIRRawDataKind::Int);
      out_name.ptr[i] = data._int;
    }

    // Finish
    state->analyser->environment->link_libraries.push({
        .name = out_name,
        .scope = (LibraryScope)scope_arg->data._int,
    });
    frame->inject({.data = {.kind = UIRRawDataKind::Void}});
  } else if (name.compare("linkDirectory")) {
    assert(frame->values.length == 1);

    // Get Arguments
    UIRLiteral *path_arg =
        *frame->values.getPtrUnchecked(frame->values.length - 2);
    assert(path_arg->data.kind == UIRRawDataKind::Slice);

    // Values to String
    String out_path = {
        .ptr = state->allocator->alloc(path_arg->data.values.len),
        .len = path_arg->data.values.len,
    };
    for (size_t i = 0; i < path_arg->data.values.len; i++) {
      UIRRawData data = path_arg->data.values[i];
      assert(data.kind == UIRRawDataKind::Int);
      out_path.ptr[i] = data._int;
    }

    // Finish
    state->analyser->environment->link_directories.push(out_path);
    frame->inject({.data = {.kind = UIRRawDataKind::Void}});
  } else if (name.compare("linkerScript")) {
    assert(frame->values.length == 1);

    // Get Arguments
    UIRLiteral *path_arg =
        *frame->values.getPtrUnchecked(frame->values.length - 2);
    assert(path_arg->data.kind == UIRRawDataKind::Slice);

    // Values to String
    String out_path = {
        .ptr = state->allocator->alloc(path_arg->data.values.len),
        .len = path_arg->data.values.len,
    };
    for (size_t i = 0; i < path_arg->data.values.len; i++) {
      UIRRawData data = path_arg->data.values[i];
      assert(data.kind == UIRRawDataKind::Int);
      out_path.ptr[i] = data._int;
    }

    // Finish
    state->analyser->environment->linker_scripts.push(out_path);
    frame->inject({.data = {.kind = UIRRawDataKind::Void}});
  } else if (name.compare("linkerFlags")) {
    assert(frame->values.length == 1);

    // Get Arguments
    UIRLiteral *flags_arg =
        *frame->values.getPtrUnchecked(frame->values.length - 2);
    assert(flags_arg->data.kind == UIRRawDataKind::Slice);

    // Values to String
    String out_flags = {
        .ptr = state->allocator->alloc(flags_arg->data.values.len),
        .len = flags_arg->data.values.len,
    };
    for (size_t i = 0; i < flags_arg->data.values.len; i++) {
      UIRRawData data = flags_arg->data.values[i];
      assert(data.kind == UIRRawDataKind::Int);
      out_flags.ptr[i] = data._int;
    }

    // Finish
    state->analyser->environment->linker_flags.push(out_flags);
    frame->inject({.data = {.kind = UIRRawDataKind::Void}});
  }
}
