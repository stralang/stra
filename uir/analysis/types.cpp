#include "../uir.hpp"
#include "analysis.hpp"
#include "define.hpp"
#include "rir/type.hpp"
#include "uir/literal.hpp"

bool compareRawDataToType(UIRAnalyser *analyser, UIRRawDataKind kind,
                          RIRTypeId type_id) {
  RIRType *type = analyser->rir_ctx->getType(type_id);
  switch (type->kind) {
  case RIRTypeKind::Void: {
    return kind == UIRRawDataKind::Void;
  }
  case RIRTypeKind::Bool: {
    return kind == UIRRawDataKind::Bool;
  }
  case RIRTypeKind::Integer: {
    return kind == UIRRawDataKind::Int;
  }
  case RIRTypeKind::Float: {
    return kind == UIRRawDataKind::Float;
  }
  case RIRTypeKind::Pointer: {
    return kind == UIRRawDataKind::Pointer;
  }
  case RIRTypeKind::Slice: {
    return kind == UIRRawDataKind::Slice;
  }
  case RIRTypeKind::TypeId: {
    return kind == UIRRawDataKind::TypeId;
  }
  }

  return false;
}

RIRValue *autoCast(UIRAnalyser *analyser, RIRValue *src, RIRTypeId dst) {
  RIRType *src_type = analyser->rir_ctx->getType(src->result);
  RIRType *dst_type = analyser->rir_ctx->getType(dst);

  if (src_type->kind == RIRTypeKind::Pointer &&
      dst_type->kind == RIRTypeKind::Slice) {
    RIRType *src_child = analyser->rir_ctx->getType(src_type->child);
    if (dst_type->slice.length < 0 &&
        src_child->compare(analyser->rir_ctx->types, dst_type->slice.child)) {
      // Pointer to Pointer Slice
      RIRValueId out_id = analyser->builder.buildCast(src->id, dst, true);
      return analyser->rir_ctx->getInst(out_id);
    }
  }

  return src;
}

RIRConstant uirRawDataToRIRConstant(UIRRawData raw_data) {
  RIRConstant rir_const;
  switch (raw_data.kind) {
  case UIRRawDataKind::Bool: {
    rir_const.kind = RIRConstantKind::Bool;
    rir_const._bool = raw_data._bool;
    break;
  }
  case UIRRawDataKind::Int: {
    rir_const.kind = RIRConstantKind::Integer;
    rir_const.integer = raw_data._int;
    break;
  }
  case UIRRawDataKind::Float: {
    rir_const.kind = RIRConstantKind::Float;
    rir_const._float = raw_data._float;
    break;
  }
  }
  return rir_const;
}
