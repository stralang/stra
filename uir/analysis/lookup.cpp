#include "../literal.hpp"
#include "../uir.hpp"
#include "common/debug.hpp"
#include "define.hpp"
#include "rir/rir.hpp"
#include "rir/type.hpp"
#include "uir/analysis/analysis.hpp"
#include "uir/comptime/define.hpp"
#include <cassert>

Option<UIRResolved> analyseLookup(UIRAnalyser *analyser, UIRValue *inst) {
  // Get parent info
  UIRValue *parent_inst = analyser->ctx->getInst(inst->lookup.parent);
  analyse(analyser, parent_inst);
  UIRResolved parent_resolved =
      *analyser->resolved_mapping.get(inst->lookup.parent);
  if (parent_resolved.kind == UIRResolvedKind::Pending ||
      parent_resolved.kind == UIRResolvedKind::Poison) {
    return parent_resolved;
  }

  if (parent_resolved.kind == UIRResolvedKind::Literal &&
      parent_resolved.literal.data.kind == UIRRawDataKind::Pointer &&
      parent_resolved.literal.data.ptr.kind == UIRPlaceKind::Inst) {
    parent_resolved = {.kind = UIRResolvedKind::Inst,
                       .inst = parent_resolved.literal.data.ptr.inst};
  } else if (parent_resolved.kind == UIRResolvedKind::Literal) {
    analyser->comptime_state.pushStack();
    Option<UIRLiteral> opt_lit =
        executeLookupPtr(&analyser->comptime_state,
                         analyser->comptime_state.currentStack(), inst);
    analyser->comptime_state.popStack();

    if (opt_lit.isNone()) {
      Option<UIRResolved> out = UIRResolved{};
      out.setNone();
      return out;
    }

    UIRLiteral lit = opt_lit.get();
    if (lit.data.kind == UIRRawDataKind::Pointer &&
        lit.data.ptr.kind == UIRPlaceKind::Inst) {
      return UIRResolved{.kind = UIRResolvedKind::Inst,
                         .inst = lit.data.ptr.inst};
    }

    return UIRResolved{.kind = UIRResolvedKind::Literal, .literal = lit};
  }

  RIRValue *ptr_inst = analyser->rir_ctx->getInst(parent_resolved.inst);
  RIRType *original_ptr_type = analyser->rir_ctx->getType(ptr_inst->result);
  RIRType *ptr_type =
      analyser->rir_ctx->getType(original_ptr_type->pointer.child);

  if (ptr_type->kind == RIRTypeKind::Pointer) {
    RIRValueId ptr_inst_id =
        analyser->builder.buildLoad(ptr_inst->id, ptr_type->id);
    ptr_inst = analyser->rir_ctx->getInst(ptr_inst_id);
    ptr_type = analyser->rir_ctx->getType(ptr_type->pointer.child);
  }

  // Search
  String *member = &inst->lookup.member;
  UIRScope *definitions = nullptr;

  Option<RIRTypeId> sub_type = {};
  bool const_result = false;
  size_t field_index = 0;

  if (ptr_type->kind == RIRTypeKind::Slice) {
    if (member->compare("ptr")) {
      sub_type = analyser->rir_ctx->types->push({
          .kind = RIRTypeKind::Slice,
          .slice = {.length = -1, .child = ptr_type->slice.child},
      });
      field_index = 0;
      const_result = true;
    } else if (member->compare("len")) {
      sub_type = analyser->rir_ctx->types->push({
          .kind = RIRTypeKind::Integer,
          .integer = {false, -1},
      });
      field_index = 1;
      const_result = true;
    }

    // Compile-time length
    if (field_index == 1 && ptr_type->slice.length > 0) {
      UIRRawData *len_data =
          (UIRRawData *)analyser->allocator->alloc(sizeof(UIRRawData));
      *len_data = {.kind = UIRRawDataKind::Int, ._int = ptr_type->slice.length};

      UIRRawData result_data = {.kind = UIRRawDataKind::Pointer};
      result_data.ptr = {
          .kind = UIRPlaceKind::Raw,
          .data = len_data,
      };

      RIRTypeId result_type = analyser->rir_ctx->types->push({
          .kind = RIRTypeKind::Pointer,
          .pointer = {sub_type.get(), true},
      });
      return UIRResolved{
          .kind = UIRResolvedKind::Literal,
          .literal = {.data = result_data, .lit_type = result_type},
      };
    }
  } else if (ptr_type->kind == RIRTypeKind::Struct) {
    UIRValue *struct_inst = analyser->type_extras.get(ptr_type->id)->creator;
    definitions = analyser->ctx->getScope(struct_inst->_struct.definitions);

    for (size_t i = 0; i < struct_inst->_struct.fields.len; i++) {
      UIRStruct::Field *field = struct_inst->_struct.fields.ptr + i;
      if (field->name.compare(*member)) {
        sub_type = ptr_type->_struct.fields.ptr[i];
        const_result = ptr_type->_struct.consts.ptr[i];
        field_index = i;
        break;
      }
    }
  } else if (ptr_type->kind == RIRTypeKind::Enum) {
    UIRValue *enum_inst = analyser->type_extras.get(ptr_type->id)->creator;
    definitions = analyser->ctx->getScope(enum_inst->_enum.definitions);
  } else if (ptr_type->kind == RIRTypeKind::Union) {
    UIRValue *union_inst = analyser->type_extras.get(ptr_type->id)->creator;
    definitions = analyser->ctx->getScope(union_inst->_union.definitions);

    if (member->compare("tag")) {
      if (analyser->ctx->getType(ptr_type->_union.repr)->kind ==
          RIRTypeKind::Void) {
        error(inst->source_location, "Cannot get tag of tag-less union");
        Option<UIRResolved> out = UIRResolved{};
        out.setNone();
        return out;
      }

      sub_type = ptr_type->_union.repr;
      field_index = 0;
    } else {
      for (size_t i = 0; i < union_inst->_union.variants.len; i++) {
        UIRStruct::Field *variant = union_inst->_union.variants.ptr + i;
        if (variant->name.compare(*member)) {
          sub_type = ptr_type->_union.variants.ptr[i];
          field_index = i;
          break;
        }
      }

      if (sub_type.isSome()) {
        RIRTypeId result_type = analyser->rir_ctx->types->push({
            .kind = RIRTypeKind::Pointer,
            .pointer = {sub_type.get(), false},
        });
        RIRValueId out_id;
        if (inst->lookup.precedes_store) {
          if (original_ptr_type->pointer.const_pointee) {
            error(inst->source_location,
                  "Cannot update tag for constant union");

            Option<UIRResolved> out = UIRResolved{};
            out.setNone();
            return out;
          }

          out_id = analyser->builder.buildSetVariant(ptr_inst->id, field_index,
                                                     result_type);
        } else {
          out_id = analyser->builder.buildGetVariant(ptr_inst->id, field_index,
                                                     result_type);
        }
        return UIRResolved{
            .kind = UIRResolvedKind::Inst,
            .inst = out_id,
        };
      }
    }
  }

  if (sub_type.isSome()) {
    RIRTypeId result_type = analyser->rir_ctx->types->push({
        .kind = RIRTypeKind::Pointer,
        .pointer = {sub_type.get(), const_result},
    });
    RIRValueId out_id =
        analyser->builder.buildFieldAt(ptr_inst->id, field_index, result_type);
    return UIRResolved{
        .kind = UIRResolvedKind::Inst,
        .inst = out_id,
    };
  }

  // Definitions
  if (definitions != nullptr) {
    for (size_t i = 0; i < definitions->list.length; i++) {
      UIRValue *child =
          analyser->ctx->getInst(definitions->list.getUnchecked(i));
      if (child->name.compare(*member)) {
        analyse(analyser, child);
        return *analyser->resolved_mapping.get(child->id);
      }
    }
  }

  Option<UIRResolved> out = UIRResolved{};
  out.setNone();
  return out;
}

void analyseLookupPtr(UIRAnalyser *analyser, UIRValue *inst) {
  Option<UIRResolved> opt_resolved = analyseLookup(analyser, inst);
  if (opt_resolved.isNone()) {
    // Error
    expect(false, inst->source_location,
           "Couldn't find member of name \"" << inst->lookup.member << "\"");
    return;
  }

  UIRResolved resolved = opt_resolved.get();
  checkResolved((&resolved));

  analyser->resolved_mapping.insert(inst->id, resolved);
}

void analyseLookupValue(UIRAnalyser *analyser, UIRValue *inst) {
  Option<UIRResolved> opt_resolved = analyseLookup(analyser, inst);
  if (opt_resolved.isSome()) {
    UIRResolved resolved = opt_resolved.get();
    checkResolved((&resolved));

    // Instruction pointer
    Option<RIRValueId> result_inst_id;
    if (resolved.kind == UIRResolvedKind::Inst) {
      result_inst_id = resolved.inst;
    } else if (resolved.literal.data.kind == UIRRawDataKind::Pointer &&
               resolved.literal.data.ptr.kind == UIRPlaceKind::Inst) {
      result_inst_id = resolved.literal.data.ptr.inst;
    }

    if (result_inst_id.isSome()) {
      RIRValue *result_inst = analyser->rir_ctx->getInst(result_inst_id.get());
      RIRType *result_type = analyser->rir_ctx->getType(result_inst->result);
      RIRValueId loaded_result = analyser->builder.buildLoad(
          result_inst->id, result_type->pointer.child);

      analyser->resolved_mapping.insert(
          inst->id, {.kind = UIRResolvedKind::Inst, .inst = loaded_result});
      return;
    }

    // Literal
    assert(resolved.kind == UIRResolvedKind::Literal);

    UIRLiteral result_lit = resolved.literal;
    if (result_lit.data.kind == UIRRawDataKind::Pointer) {
      assert(result_lit.data.ptr.kind == UIRPlaceKind::Raw);
      result_lit.data = *result_lit.data.ptr.data;
      if (result_lit.lit_type.isSome()) {
        result_lit.lit_type =
            analyser->rir_ctx->getType(result_lit.lit_type.get())
                ->pointer.child;
      }
    }

    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Literal, .literal = result_lit});
    return;
  }

  analyser->comptime_state.pushStack();
  Option<UIRLiteral> out_lit = executeLookupValue(
      &analyser->comptime_state, analyser->comptime_state.currentStack(), inst);
  analyser->comptime_state.popStack();

  if (out_lit.isNone()) {
    // Error
    expect(false, inst->source_location,
           "Couldn't find member of name \"" << inst->lookup.member << "\"");
    return;
  }

  analyser->resolved_mapping.insert(
      inst->id, {.kind = UIRResolvedKind::Literal, .literal = out_lit.get()});
}
