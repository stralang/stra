#include "define.hpp"
#include "literal.hpp"
#include "uir.hpp"

Type *getFieldType(UIRValue *_struct, String name) {
  for (size_t i = 0; i < _struct->_struct.fields.len; i++) {
    UIRStruct::Field *field = _struct->_struct.fields.ptr + i;
    if (field->name.compare(name)) {
      return field->type->literal._typeid;
    }
  }

  return nullptr;
}

void analyseAggregate(UIRAnalyser *analyser, UIRModule *module,
                      UIRValue *inst) {
  assert(inst->kind == UIRValueKind::Aggregate);

  // Get Type
  UIRLiteral type_lit =
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

    UIRValue *value = inst->aggregate.values.ptr[i];
    analyse(analyser, module, value);

    autoCast(analyser, value, field_type);
    expect(compareTypes(value->result_type, field_type), value->source_location,
           "Initializer value doesn't match element type");
  }

  // TODO: Convert named initializer into list initializer;
  // And include struct defaults when those are added
}
