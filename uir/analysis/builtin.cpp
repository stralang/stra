#include "common/debug.hpp"
#include "define.hpp"
#include "rir/type.hpp"
#include "uir/literal.hpp"

UIRLiteral getBuiltinGlobalValue(UIRAnalyser *analyser, UIRValue *inst) {
  String name = inst->name;
  if (inst->global_variable.attributes.builtin.isInline()) {
    name = inst->global_variable.attributes.builtin.get();
  } else if (inst->global_variable.attributes.link_name.isInline()) {
    name = inst->global_variable.attributes.link_name.get();
  }

  UIRLiteral out = {};
  if (name.compare("TARGET_ARCH")) {
    out.data = {.kind = UIRRawDataKind::Int,
                ._int = (int64_t)analyser->environment->target.arch};
  } else if (name.compare("TARGET_OS")) {
    out.data = {.kind = UIRRawDataKind::Int,
                ._int = (int64_t)analyser->environment->target.os};
  } else if (name.compare("TARGET_SUB_OS")) {
    out.data = {.kind = UIRRawDataKind::Int,
                ._int = (int64_t)analyser->environment->target.sub_os};
  } else if (name.compare("TARGET_VENDOR")) {
    String vendor = analyser->environment->target.vendor;
    Slice<UIRRawData> s = {
        .ptr = (UIRRawData *)analyser->allocator->alloc(sizeof(UIRRawData) *
                                                        vendor.len),
        .len = vendor.len,
    };

    for (size_t i = 0; i < vendor.len; i++) {
      s[i] = {.kind = UIRRawDataKind::Int, ._int = vendor.ptr[i]};
    }

    out.data = {.kind = UIRRawDataKind::Slice, .values = s};

    RIRTypeId elem_ty = analyser->ctx->types->push(
        {.kind = RIRTypeKind::Integer, .integer = {false, 8}});
    out.lit_type = analyser->ctx->types->push({
        .kind = RIRTypeKind::Slice,
        .slice = {.length = (int64_t)analyser->environment->target.vendor.len,
                  .child = elem_ty},
    });
  } else if (name.compare("TARGET_ENDIAN")) {
    out.data = {.kind = UIRRawDataKind::Int,
                ._int = (int64_t)analyser->environment->endianness};
  } else {
    expect(false, inst->source_location,
           "Builtin variable `" << name << "` doesn't exist\n");
  }

  return out;
}
