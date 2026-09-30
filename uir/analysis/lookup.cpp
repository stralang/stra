#include "../literal.hpp"
#include "../uir.hpp"
#include "common/debug.hpp"
#include "define.hpp"
#include "rir/type.hpp"
#include "uir/analysis/analysis.hpp"

UIRResolved analyseLookup(UIRAnalyser *analyser, UIRModule *module,
                          RIRValueId ptr_id, String *member,
                          SrcLoc source_location) {
  RIRValue *ptr_inst = analyser->rir_ctx->getInst(ptr_id);
  RIRType *ptr_type = analyser->rir_ctx->getType(ptr_inst->result);
  expect(ptr_type->kind == RIRTypeKind::Pointer, source_location,
         "Cannot `lookup` for non-pointer");

  RIRType *parent_type = analyser->rir_ctx->getType(ptr_type->child);

  // Auto dereference
  if (parent_type->kind == RIRTypeKind::Pointer) {
    ptr_type = parent_type;
    RIRValueId ptr_inst_id =
        analyser->builder.buildLoad(ptr_inst->id, ptr_type->id);
    ptr_inst = analyser->rir_ctx->getInst(ptr_inst_id);
    parent_type = analyser->rir_ctx->getType(parent_type->child);
  }

  // Search
  UIRScope *definitions = nullptr;
  Option<RIRTypeId> sub_type = {};
  size_t field_index = 0;

  if (parent_type->kind == RIRTypeKind::Slice) {
    if (member->compare("ptr")) {
      sub_type = analyser->rir_ctx->types->push({
          .kind = RIRTypeKind::Pointer,
          .child = parent_type->slice.child,
      });
      field_index = 0;
    } else if (member->compare("len")) {
      sub_type = analyser->rir_ctx->types->push({
          .kind = RIRTypeKind::Integer,
          .integer = {false, -1},
      });
      field_index = 1;
    }

    // Compile-time length
    if (field_index == 1 && parent_type->slice.length > 0) {
      UIRRawData *len_data =
          (UIRRawData *)analyser->allocator->alloc(sizeof(UIRRawData));
      *len_data = {.kind = UIRRawDataKind::Int,
                   ._int = parent_type->slice.length};

      UIRRawData ptr_data = {.kind = UIRRawDataKind::Pointer};
      ptr_data.ptr = {
          .kind = UIRPlaceKind::Raw,
          .data = len_data,
      };

      RIRTypeId ptr_type = analyser->rir_ctx->types->push(
          {.kind = RIRTypeKind::Pointer, .child = sub_type.get()});
      return {
          .kind = UIRResolvedKind::Literal,
          .literal = {.data = ptr_data, .lit_type = ptr_type},
      };
    }
  } else if (parent_type->kind == RIRTypeKind::Struct) {
    UIRValue *struct_inst =
        reinterpret_cast<UIRValue *>(parent_type->_struct.unique);
    definitions = struct_inst->_struct.definitions;

    for (size_t i = 0; i < struct_inst->_struct.fields.len; i++) {
      UIRStruct::Field *field = struct_inst->_struct.fields.ptr + i;
      if (field->name.compare(*member)) {
        sub_type = parent_type->_struct.fields.ptr[i];
        field_index = i;
        break;
      }
    }
  } else if (parent_type->kind == RIRTypeKind::Enum) {
    UIRValue *enum_inst =
        reinterpret_cast<UIRValue *>(parent_type->_enum.unique);
    definitions = enum_inst->_enum.definitions;
  }

  if (sub_type.isSome()) {
    RIRTypeId result_type = analyser->rir_ctx->types->push({
        .kind = RIRTypeKind::Pointer,
        .child = sub_type.get(),
    });
    RIRValueId out_id =
        analyser->builder.buildFieldAt(ptr_inst->id, field_index, result_type);
    return {
        .kind = UIRResolvedKind::Inst,
        .inst = out_id,
    };
  }

  // Definitions
  if (definitions != nullptr) {
    for (size_t i = 0; i < definitions->list.length; i++) {
      UIRValue *child = definitions->list.getUnchecked(i);
      if (child->name.compare(*member)) {
        analyse(analyser, module, child);
        return *analyser->resolved_mapping.get(child);
      }
    }
  }

  // Error
  expect(false, source_location,
         "Couldn't find member of name \"" << *member << "\"");
  return {};
}

void analyseLookupPtr(UIRAnalyser *analyser, UIRModule *module,
                      UIRValue *inst) {
  analyse(analyser, module, inst->lookup.parent);
  UIRResolved *parent_resolved =
      analyser->resolved_mapping.get(inst->lookup.parent);

  // Instruction parent
  if (parent_resolved->kind == UIRResolvedKind::Inst) {
    UIRResolved result = analyseLookup(analyser, module, parent_resolved->inst,
                                       &inst->lookup.member,
                                       inst->lookup.parent->source_location);
    analyser->resolved_mapping.insert(inst, result);
    return;
  }

  // Literal parent
  assert(parent_resolved->kind == UIRResolvedKind::Literal);
  UIRLiteral lit = analyser->comptime_state.execute(module, inst);
  analyser->resolved_mapping.insert(
      inst, {.kind = UIRResolvedKind::Literal, .literal = lit});
}

void analyseLookupValue(UIRAnalyser *analyser, UIRModule *module,
                        UIRValue *inst) {
  analyse(analyser, module, inst->lookup.parent);
  UIRResolved *parent_resolved =
      analyser->resolved_mapping.get(inst->lookup.parent);

  // Instruction parent
  if (parent_resolved->kind == UIRResolvedKind::Inst) {
    UIRResolved result = analyseLookup(analyser, module, parent_resolved->inst,
                                       &inst->lookup.member,
                                       inst->lookup.parent->source_location);
    if (result.kind == UIRResolvedKind::Inst) {
      RIRValue *out_lookup = analyser->rir_ctx->getInst(result.inst);
      RIRType *lookup_result = analyser->rir_ctx->getType(out_lookup->result);

      RIRValueId out_id =
          analyser->builder.buildLoad(out_lookup->id, lookup_result->child);
      analyser->resolved_mapping.insert(
          inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
      return;
    }

    assert(result.kind == UIRResolvedKind::Literal);
    if (result.literal.data.ptr.kind == UIRPlaceKind::Inst) {
      analyser->resolved_mapping.insert(
          inst, {
                    .kind = UIRResolvedKind::Inst,
                    .inst = result.literal.data.ptr.inst,
                });
    } else if (result.literal.data.ptr.kind == UIRPlaceKind::Raw) {
      UIRLiteral out_lit = {.data = *result.literal.data.ptr.data};
      if (result.literal.lit_type.isSome()) {
        RIRType *ptr_type =
            analyser->rir_ctx->getType(result.literal.lit_type.get());
        out_lit.lit_type = ptr_type->child;
      }

      analyser->resolved_mapping.insert(
          inst, {.kind = UIRResolvedKind::Literal, .literal = out_lit});
    }
    return;
  }

  // Literal parent
  assert(parent_resolved->kind == UIRResolvedKind::Literal);
  UIRLiteral lit = analyser->comptime_state.execute(module, inst);
  analyser->resolved_mapping.insert(
      inst, {.kind = UIRResolvedKind::Literal, .literal = lit});
}
