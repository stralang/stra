#include "comptime.hpp"
#include "literal.hpp"
#include "mir.hpp"
#include <cstdlib>
#include <iostream>

MIRLiteral execute(MIRComptime *state, MIRModule *module, MIRValue *inst) {
  switch (inst->kind) {
  case MIRValueKind::Literal: {
    return inst->literal;
  }
  case MIRValueKind::Function: {
    // Analyse Type
    Type raw_type = {.kind = TypeKind::Function, .is_constant = true};
    raw_type.function.arguments = {
        .ptr = (Type **)state->arena->alloc(sizeof(MIRValue) *
                                            inst->function.parameter_types.len),
        .len = inst->function.parameter_types.len,
    };

    // Analyse Parameters
    for (size_t i = 0; i < inst->function.parameter_types.len; i++) {
      MIRValue *param = inst->function.parameter_types.ptr[i];

      MIRLiteral param_literal = execute(state, module, param);
      raw_type.function.arguments.ptr[i] = param_literal._typeid;
    }

    // Analyse Return Type
    MIRLiteral return_literal =
        execute(state, module, inst->function.return_type);
    raw_type.function.return_type = return_literal._typeid;

    // Get final type
    MIRLiteral out_lit = {
        .lit_type = module->ctx->type_cache->get(raw_type),
    };
    if (inst->function.globals == nullptr && !inst->function.undefined) {
      out_lit._typeid = out_lit.lit_type;
      out_lit.lit_type =
          module->ctx->type_cache->get({.kind = TypeKind::TypeId});
    } else {
      out_lit.function = inst;
    }
    return out_lit;
  }
  }

  std::cerr << "TODO: Implement compile-time execution of `" << std::hex
            << (uint16_t)inst->kind << "`\n";
  std::abort();
}

MIRLiteral MIRComptime::execute(MIRModule *module, MIRValue *inst) {
  return ::execute(this, module, inst);
}
