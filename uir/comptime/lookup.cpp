#include "common/debug.hpp"
#include "define.hpp"
#include "uir/analysis/analysis.hpp"
#include "uir/literal.hpp"
#include <cassert>

UIRLiteral executeLookup(UIRComptime *state, UIRModule *module,
                         ComptimeStackFrame *frame, UIRLiteral parent_lit,
                         String *member) {
  assert(0 && "TODO: Implement lookup for compile-time execution");
}

UIRLiteral executeLookupPtr(UIRComptime *state, UIRModule *module,
                            ComptimeStackFrame *frame, UIRValue *inst) {
  UIRLiteral parent_lit = state->getValue(frame, module, inst->lookup.parent);
  return executeLookup(state, module, frame, parent_lit, &inst->lookup.member);
}

UIRLiteral executeLookupValue(UIRComptime *state, UIRModule *module,
                              ComptimeStackFrame *frame, UIRValue *inst) {
  UIRLiteral parent_lit = state->getValue(frame, module, inst->lookup.parent);

  // FIXME: This ptr->ptr should be handled above
  if (parent_lit.data.ptr.kind == UIRPlaceKind::Raw &&
      parent_lit.data.ptr.data->kind == UIRRawDataKind::TypeId) {
    RIRTypeId parent_type_id = parent_lit.data.ptr.data->_typeid;
    RIRType *parent_type = state->analyser->rir_ctx->getType(parent_type_id);

    // Get Enum Value
    if (parent_type->kind == RIRTypeKind::Enum) {
      UIRExtraTypeInfo *extras =
          state->analyser->type_extras.get(parent_type->id);
      UIRValue *enum_inst = extras->creator;

      for (size_t i = 0; i < enum_inst->_enum.members.len; i++) {
        UIREnum::Member *enum_member = enum_inst->_enum.members.ptr + i;
        if (enum_member->name.compare(inst->lookup.member)) {
          return extras->constants.ptr[i];
        }
      }
    }
  }

  // Load literal
  UIRLiteral lit =
      executeLookup(state, module, frame, parent_lit, &inst->lookup.member);
  assert(lit.data.kind == UIRRawDataKind::Pointer);

  UIRPlace place = lit.data.ptr;
  assert(place.kind == UIRPlaceKind::Raw);
  return {.data = *place.data, .lit_type = lit.lit_type};
}
