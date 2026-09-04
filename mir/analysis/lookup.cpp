#include "../../src/print.hpp"
#include "define.hpp"
#include "literal.hpp"
#include "mir.hpp"

void analyseLookup(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst) {
  analyse(analyser, module, inst->lookup.parent);
  expect(inst->lookup.parent->result_type->kind == TypeKind::Pointer,
         inst->lookup.parent->source_location,
         "Cannot `lookup` for non-pointer");

  Type *parent_ty = inst->lookup.parent->result_type->child;
  bool is_typeid = parent_ty->kind == TypeKind::TypeId;
  if (is_typeid) {
    MIRLiteral ty_lit =
        analyser->comptime_state.execute(module, inst->lookup.parent);
    parent_ty = ty_lit.pointer->_typeid;
  }

  MIRScope *definitions = nullptr;
  if (parent_ty->kind == TypeKind::Namespace) {
    definitions = parent_ty->_namespace.inst->_namespace.definitions;
  }

  for (size_t i = 0; i < definitions->list.length; i++) {
    MIRValue *child = definitions->list.getUnchecked(i);
    analyse(analyser, module, child);
    if (child->name.compare(inst->lookup.member)) {
      inst->kind = MIRValueKind::Alias;
      inst->alias = child;
      inst->result_type = child->result_type;
      return;
    }
  }
}
