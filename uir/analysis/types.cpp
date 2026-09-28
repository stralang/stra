#include "../uir.hpp"
#include "analysis.hpp"
#include "define.hpp"

// Attempts to NO-OP convert `src` to `dst`
Type *autoConvert(UIRAnalyser *analyser, Type *src, Type *dst) {
  // FIXME:
  // if (src->kind == TypeKind::Integer && src->integer.is_untyped &&
  //     dst->kind == TypeKind::Integer) {
  //   if (dst->integer.is_signed || !src->integer.is_signed) {
  //     return dst;
  //   }
  // } else if (src->kind == TypeKind::Float && src->_float.is_untyped &&
  //            dst->kind == TypeKind::Float) {
  //   return dst;
  // } else if (src->kind == TypeKind::Pointer && dst->kind == TypeKind::Slice
  // &&
  //            dst->slice.length < 0 &&
  //            compareTypes(src->child, dst->slice.type)) {
  //   // Pointer to Pointer Slice
  //   return dst;
  // }
  //
  return src;
}

void fixUntyped(UIRAnalyser *analyser, UIRValue *inst, Type *real) {
  // FIXME:
  // if (!(inst->result_type->kind == TypeKind::Integer &&
  //       inst->result_type->integer.is_untyped) &&
  //     !(inst->result_type->kind == TypeKind::Float &&
  //       inst->result_type->_float.is_untyped)) {
  //   return;
  // }
  //
  // inst->result_type = real;
  // switch (inst->kind) {
  // case UIRValueKind::BinOp: {
  //   fixUntyped(analyser, inst->binop.lhs, real);
  //   fixUntyped(analyser, inst->binop.rhs, real);
  //   break;
  // }
  // case UIRValueKind::UnaryOp: {
  //   fixUntyped(analyser, inst->unaryop.value, real);
  //   break;
  // }
  // case UIRValueKind::Literal: {
  //   inst->literal.lit_type = real;
  //   break;
  // }
  // }
}

void autoCast(UIRAnalyser *analyser, UIRValue *src, Type *dst) {
  // FIXME:
  // if ((src->result_type->kind == TypeKind::Integer &&
  //      src->result_type->integer.is_untyped) ||
  //     (src->result_type->kind == TypeKind::Float &&
  //      src->result_type->_float.is_untyped)) {
  //   fixUntyped(analyser, src, dst);
  // }
  //
  // src->result_type = autoConvert(analyser, src->result_type, dst);
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
