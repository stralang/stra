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
      expected_type = aggregate_type->_struct.fields.ptr[index];
    } else {
      expected_type = aggregate_type->slice.child;
    }

    // Get Value
    UIRValue *value = inst->aggregate.values.ptr[i];
    analyse(analyser, module, value);

    UIRResolved *value_resolved = analyser->resolved_mapping.get(value);
    RIRValueId out_id =
        getInstFromResolved(analyser, value_resolved, expected_type);

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
