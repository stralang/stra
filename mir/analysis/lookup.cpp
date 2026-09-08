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
  if (parent_ty->kind == TypeKind::Slice) {
    Type *sub_type = nullptr;
    if (inst->lookup.member.compare("ptr")) {
      sub_type = module->ctx->type_cache->get({
          .kind = TypeKind::Pointer,
          .child = parent_ty->slice.type,
          .is_constant = true,
      });
    } else if (inst->lookup.member.compare("len")) {
      sub_type = module->ctx->type_cache->get({
          .kind = TypeKind::Integer,
          .integer = {false, false, -1},
          .is_constant = true,
      });
    }

    if (sub_type != nullptr) {
      inst->result_type = module->ctx->type_cache->get({
          .kind = TypeKind::Pointer,
          .child = sub_type,
          .is_constant = true,
      });
      return;
    }
  } else if (parent_ty->kind == TypeKind::Struct) {
    MIRValue *struct_inst = parent_ty->_struct.inst;
    definitions = struct_inst->_struct.definitions;

    for (size_t i = 0; i < struct_inst->_struct.fields.len; i++) {
      MIRStruct::Field *field = struct_inst->_struct.fields.ptr + i;
      if (field->name.compare(inst->lookup.member)) {
        inst->result_type = module->ctx->type_cache->get({
            .kind = TypeKind::Pointer,
            .child = field->type->literal._typeid,
            .is_constant = true,
        });
        return;
      }
    }
  } else if (parent_ty->kind == TypeKind::Namespace) {
    definitions = parent_ty->_namespace.inst->_namespace.definitions;
  }

  // Definitions
  if (definitions != nullptr) {
    for (size_t i = 0; i < definitions->list.length; i++) {
      MIRValue *child = definitions->list.getUnchecked(i);
      if (child->name.compare(inst->lookup.member)) {
        analyse(analyser, module, child);
        inst->kind = MIRValueKind::Alias;
        inst->alias = child;
        inst->result_type = child->result_type;
        return;
      }
    }
  }

  // Error
  expect(false, inst->source_location,
         "Couldn't find member of name \"" << inst->lookup.member << "\"");
}
