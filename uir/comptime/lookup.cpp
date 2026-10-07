#include "common/debug.hpp"
#include "define.hpp"
#include "uir/analysis/analysis.hpp"
#include "uir/literal.hpp"
#include <cassert>

Option<UIRLiteral> executeLookup(UIRComptime *state, ComptimeStackFrame *frame,
                                 UIRLiteral parent_lit, String *member) {
  Option<Slice<UIRValueId>> opt_definitions;
  if (parent_lit.data.kind == UIRRawDataKind::Namespace) {
    UIRValue *_namespace = parent_lit.data._namespace;
    UIRScope *definitions =
        state->ctx->getScope(_namespace->_namespace.definitions);
    opt_definitions = definitions->list.slice();
  }

  // Definitions
  if (opt_definitions.isSome()) {
    Slice<UIRValueId> definitions = opt_definitions.get();
    for (size_t i = 0; i < definitions.len; i++) {
      UIRValue *child = state->ctx->getInst(definitions.ptr[i]);
      if (child->name.compare(*member)) {
        return state->getValue(frame, child->id);
      }
    }
  }

  return {};
}

Option<UIRLiteral> executeLookupPtr(UIRComptime *state,
                                    ComptimeStackFrame *frame, UIRValue *inst) {
  UIRLiteral parent_lit = state->getValue(frame, inst->lookup.parent);
  return executeLookup(state, frame, parent_lit, &inst->lookup.member);
}

Option<UIRLiteral> executeLookupValue(UIRComptime *state,
                                      ComptimeStackFrame *frame,
                                      UIRValue *inst) {
  UIRLiteral parent_lit = state->getValue(frame, inst->lookup.parent);
  if (parent_lit.data.kind == UIRRawDataKind::Pointer) {
    if (parent_lit.data.ptr.kind == UIRPlaceKind::Raw &&
        parent_lit.data.ptr.data->kind == UIRRawDataKind::TypeId) {
      // Dereference typeid
      parent_lit = {.data = *parent_lit.data.ptr.data};
    }
  }

  if (parent_lit.data.kind == UIRRawDataKind::TypeId) {
    RIRTypeId parent_type_id = parent_lit.data._typeid;
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
  } else if (parent_lit.data.kind != UIRRawDataKind::Namespace) {
    assert(0 && "Lookup expects TypeId, Namespace, or Pointer");
  }

  // Get data
  Option<UIRLiteral> opt_lit =
      executeLookup(state, frame, parent_lit, &inst->lookup.member);
  if (opt_lit.isNone()) {
    return {};
  }

  UIRLiteral lit = opt_lit.get();
  if (lit.data.kind == UIRRawDataKind::TypeId) {
    return lit;
  }

  // Load from pointer
  assert(lit.data.kind == UIRRawDataKind::Pointer);

  UIRPlace place = lit.data.ptr;
  assert(place.kind == UIRPlaceKind::Raw);
  return UIRLiteral{.data = *place.data, .lit_type = lit.lit_type};
}
