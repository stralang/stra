#include "../analysis/analysis.hpp"
#include "define.hpp"
#include "uir/comptime/comptime.hpp"
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

  if (name.compare("sizeof")) {
    UIRLiteral **opt_lit = state->currentStack()->values.back();
    assert(opt_lit != nullptr);

    UIRLiteral *lit = *opt_lit;
    assert(lit->data.kind == UIRRawDataKind::TypeId);

    RIRType *ty = state->ctx->getType(lit->data._typeid);
    size_t size_bits = ty->sizeBits(state->ctx->types,
                                    state->analyser->environment->pointer_size);
    int64_t size = std::ceil((double)size_bits / 8.0);

    state->currentStack()->inject(
        {.data = {.kind = UIRRawDataKind::Int, ._int = size}});
  } else if (name.compare("alignof")) {
    UIRLiteral **opt_lit = state->currentStack()->values.back();
    assert(opt_lit != nullptr);

    UIRLiteral *lit = *opt_lit;
    assert(lit->data.kind == UIRRawDataKind::TypeId);

    RIRType *ty = state->ctx->getType(lit->data._typeid);
    size_t align_bits = ty->alignBits(
        state->ctx->types, state->analyser->environment->pointer_size);
    int64_t align = std::ceil((double)align_bits / 8.0);

    state->currentStack()->inject(
        {.data = {.kind = UIRRawDataKind::Int, ._int = align}});
  }
}
