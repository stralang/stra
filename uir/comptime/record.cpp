#include "define.hpp"
#include "rir/type.hpp"
#include "uir/analysis/analysis.hpp"
#include "uir/comptime/comptime.hpp"

void computeStructType(UIRComptime *state, UIRValue *inst,
                       RIRTypeId struct_type_id) {
  ComptimeStackFrame *frame = state->currentStack();

  // Setup extras
  state->analyser->type_extras.insert(struct_type_id, {.creator = inst});

  // Fields
  RIRType *struct_type = state->ctx->getType(struct_type_id);
  struct_type->_struct.fields = {
      .ptr = (RIRTypeId *)state->allocator->alloc(sizeof(RIRTypeId) *
                                                  inst->_struct.fields.len),
      .len = inst->_struct.fields.len,
  };
  struct_type->_struct.consts = {
      .ptr = (bool *)state->analyser->allocator->alloc(
          sizeof(bool) * inst->_struct.fields.len),
      .len = inst->_struct.fields.len,
  };
  for (size_t i = 0; i < inst->_struct.fields.len; i++) {
    UIRStruct::Field *field = inst->_struct.fields.ptr + i;
    UIRLiteral type_lit = state->getValue(frame, field->type);
    ctexpect(type_lit.data.kind == UIRRawDataKind::TypeId,
             inst->source_location, "Struct field type must be of TypeId");
    struct_type->_struct.fields.ptr[i] = type_lit.data._typeid;
    struct_type->_struct.consts.ptr[i] = field->is_const;
  }

  UIRLiteral *lit_out = frame->add(inst->id);
  lit_out->data = {.kind = UIRRawDataKind::TypeId, ._typeid = struct_type_id};
}

void computeEnumType(UIRComptime *state, UIRValue *inst,
                     RIRTypeId enum_type_id) {
  ComptimeStackFrame *frame = state->currentStack();

  // Setup extras
  state->analyser->type_extras.insert(enum_type_id, {.creator = inst});

  // Represent Type
  RIRType *enum_type = state->analyser->rir_ctx->getType(enum_type_id);
  UIRLiteral repr_lit = state->getValue(frame, inst->_enum.repr_type);
  enum_type->_enum.repr = repr_lit.data._typeid;

  // Members
  Slice<UIRLiteral> members = {
      .ptr = (UIRLiteral *)state->allocator->allocZeroed(
          sizeof(UIRLiteral) * inst->_enum.members.len),
      .len = inst->_enum.members.len,
  };
  int64_t next_value = 0;
  for (size_t i = 0; i < inst->_enum.members.len; i++) {
    UIREnum::Member *member = inst->_enum.members.ptr + i;

    UIRLiteral *result = members.ptr + i;
    if (member->constant.isSome()) {
      *result = **executeGetReturn(state,
                                   state->ctx->getInst(member->constant.get()));
    } else {
      result->data = {.kind = UIRRawDataKind::Int, ._int = next_value};
    }

    if (result->lit_type.isSome()) {
      ctexpect(result->lit_type.get() == enum_type->_enum.repr,
               inst->source_location,
               "Enum member constant has incorrect type. Expected `"
                   << enum_type->_enum.repr << "` got `"
                   << result->lit_type.get() << "`");
    }

    result->lit_type = enum_type_id;
    next_value = result->data._int + 1;
  }

  state->analyser->type_extras.get(enum_type_id)->constants = members;

  UIRLiteral *lit_out = frame->add(inst->id);
  lit_out->data = {.kind = UIRRawDataKind::TypeId, ._typeid = enum_type_id};
}

void computeUnionType(UIRComptime *state, UIRValue *inst,
                      RIRTypeId union_type_id) {
  ComptimeStackFrame *frame = state->currentStack();

  // Setup extras
  state->analyser->type_extras.insert(union_type_id, {.creator = inst});

  // Represent Type
  RIRType *union_type = state->analyser->rir_ctx->getType(union_type_id);
  UIRLiteral repr_lit = state->getValue(frame, inst->_union.repr_type);
  ctexpect(repr_lit.data.kind == UIRRawDataKind::TypeId, inst->source_location,
           "Union representation type must be of TypeId");
  union_type->_union.repr = repr_lit.data._typeid;

  // Fields
  union_type->_union.variants = {
      .ptr = (RIRTypeId *)state->allocator->alloc(sizeof(RIRTypeId) *
                                                  inst->_union.variants.len),
      .len = inst->_union.variants.len,
  };
  for (size_t i = 0; i < inst->_union.variants.len; i++) {
    UIRStruct::Field *field = inst->_union.variants.ptr + i;
    UIRLiteral type_lit = state->getValue(frame, field->type);
    ctexpect(type_lit.data.kind == UIRRawDataKind::TypeId,
             inst->source_location, "Union variant type must be of TypeId");
    union_type->_union.variants.ptr[i] = type_lit.data._typeid;
  }

  UIRLiteral *lit_out = frame->add(inst->id);
  lit_out->data = {.kind = UIRRawDataKind::TypeId, ._typeid = union_type_id};
}
