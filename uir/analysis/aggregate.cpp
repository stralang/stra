#include "../literal.hpp"
#include "../uir.hpp"
#include "define.hpp"
#include "rir/rir.hpp"
#include "uir/analysis/analysis.hpp"
#include <cassert>

Option<size_t> getFieldIndex(UIRValue *_struct, String name) {
  for (size_t i = 0; i < _struct->_struct.fields.len; i++) {
    UIRStruct::Field *field = _struct->_struct.fields.ptr + i;
    if (field->name.compare(name)) {
      return i;
    }
  }

  return {};
}

void analyseAggregate(UIRAnalyser *analyser, UIRModule *module,
                      UIRValue *inst) {
  assert(inst->kind == UIRValueKind::Aggregate);

  // Get Type
  analyse(analyser, module, inst->aggregate.type);
  UIRResolved *type_resolved =
      analyser->resolved_mapping.get(inst->aggregate.type);
  assert(type_resolved->kind == UIRResolvedKind::Literal);
  expect(type_resolved->literal.data.kind == UIRRawDataKind::TypeId,
         inst->aggregate.type->source_location,
         "Initializer type must be typeid");

  RIRTypeId aggregate_type_id = type_resolved->literal.data._typeid;
  RIRType *aggregate_type = analyser->rir_ctx->getType(aggregate_type_id);

  // Initial checks
  UIRValue *struct_inst = nullptr;
  if (inst->aggregate.names.ptr == nullptr) {
    expect(aggregate_type->kind == RIRTypeKind::Slice,
           inst->aggregate.type->source_location,
           "Cannot list initialize non-slice");
  } else {
    expect(aggregate_type->kind == RIRTypeKind::Struct,
           inst->aggregate.type->source_location,
           "Cannot name initialize non-struct");
    struct_inst = reinterpret_cast<UIRValue *>(aggregate_type->_struct.unique);
  }

  // Analyse Aggregate
  Slice<RIRValueId> out_values = {
      .ptr = (RIRValueId *)analyser->allocator->alloc(
          sizeof(RIRValueId) * inst->aggregate.values.len),
      .len = inst->aggregate.values.len,
  };

  for (size_t i = 0; i < inst->aggregate.values.len; i++) {
    // Get Field Index and Type
    size_t index = i;
    RIRTypeId expected_type;
    if (inst->aggregate.names.ptr != nullptr) {
      String name = inst->aggregate.names.ptr[i];
      index = getFieldIndex(struct_inst, name).get();

      UIRStruct::Field *field = struct_inst->_struct.fields.ptr + i;
      UIRResolved *field_resolved = analyser->resolved_mapping.get(field->type);
      assert(field_resolved->kind == UIRResolvedKind::Literal);
      assert(field_resolved->literal.data.kind == UIRRawDataKind::TypeId);
      expected_type = field_resolved->literal.data._typeid;
    } else {
      expected_type = aggregate_type->slice.child;
    }

    // Get Value
    UIRValue *value = inst->aggregate.values.ptr[i];
    analyse(analyser, module, value);

    UIRResolved *value_resolved = analyser->resolved_mapping.get(value);
    RIRValueId out_id;

    if (value_resolved->kind == UIRResolvedKind::Inst) {
      RIRValue *inst = analyser->rir_ctx->getInst(value_resolved->inst);
      out_id = autoCast(analyser, inst, expected_type)->id;
    } else if (value_resolved->kind == UIRResolvedKind::Literal) {
      UIRLiteral lit = value_resolved->literal;
      RIRConstant rir_const = uirRawDataToRIRConstant(lit.data);

      RIRTypeId out_type;
      if (lit.lit_type.isSome()) {
        out_type = lit.lit_type.get();
      } else {
        out_type = expected_type; // TODO: Check data kind
      }

      out_id = analyser->builder.buildConstant(out_type, rir_const);
    }

    // Compare types
    RIRValue *out_inst = analyser->rir_ctx->getInst(out_id);
    RIRType *out_type = analyser->rir_ctx->getType(out_inst->result);
    expect(out_type->compare(analyser->rir_ctx->types, expected_type),
           value->source_location,
           "Initializer value doesn't match element type");

    // Set value
    out_values[index] = out_id;
  }

  // TODO: include struct defaults when those are added

  RIRValueId out_id =
      analyser->builder.buildAggregate(aggregate_type_id, out_values);
  analyser->resolved_mapping.insert(
      inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
}
