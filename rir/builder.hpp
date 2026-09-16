#pragma once

#include "rir.hpp"

struct RIRBuilder {
  Option<RIRBlock *> block;
  RIRModule *module;

  RIRValue *build(RIRValue inst) {
    size_t idx = this->module->instructions.len();
    inst.id = idx;
    this->module->instructions.push(inst);
    return this->module->instructions.getPtrUnchecked(idx);
  }

  // RIRValue *buildLocalVariable(RIRType *type, Option<String> name = {});
  // RIRValue *buildLoad(RIRValue *ptr, Option<String> name = {});
  // RIRValue *buildStore(RIRValue *ptr, RIRValue *value,
  //                      Option<String> name = {});
};
