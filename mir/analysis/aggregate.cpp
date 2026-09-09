#include "define.hpp"
#include "literal.hpp"
#include "mir.hpp"

Type *getFieldType(MIRValue *_struct, String name) {
  for (size_t i = 0; i < _struct->_struct.fields.len; i++) {
    MIRStruct::Field *field = _struct->_struct.fields.ptr + i;
    if (field->name.compare(name)) {
      return field->type->literal._typeid;
    }
  }

  return nullptr;
}

void analyseAggregate(MIRAnalyser *analyser, MIRModule *module,
                      MIRValue *inst) {
  assert(inst->kind == MIRValueKind::Aggregate);

  // Get Type
  MIRLiteral type_lit =
      analyser->comptime_state.execute(module, inst->aggregate.type);
  expect(type_lit.lit_type->kind == TypeKind::TypeId,
         inst->aggregate.type->source_location,
         "Initializer type must be typeid");

  Type *type = type_lit._typeid;
  inst->result_type = type;

  // Initial checks
  if (inst->aggregate.names.ptr == nullptr) {
    expect(type->kind == TypeKind::Slice, inst->aggregate.type->source_location,
           "Cannot list initialize non-slice");
  } else {
    expect(type->kind == TypeKind::Struct,
           inst->aggregate.type->source_location,
           "Cannot name initialize non-struct");
  }

  // Analyse Aggregate
  for (size_t i = 0; i < inst->aggregate.values.len; i++) {
    Type *field_type = nullptr;
    if (inst->aggregate.names.ptr != nullptr) {
      String name = inst->aggregate.names.ptr[i];
      field_type = getFieldType(type->_struct.inst, name);
    } else {
      field_type = type->slice.type;
    }

    MIRValue *value = inst->aggregate.values.ptr[i];
    analyse(analyser, module, value);

    autoCast(analyser, value, field_type);
    expect(compareTypes(value->result_type, field_type), value->source_location,
           "Initializer value doesn't match element type");
  }

  // TODO: Convert named initializer into list initializer;
  // And include struct defaults when those are added
}
