#include "builder.hpp"
#include "rir.hpp"
#include <iostream>

RIRValueId RIRBuilder::build(RIRValue inst) {
  RIRValueId id = {this->module->id,
                   (uint32_t)this->module->instructions.len()};
  inst.id = id;
  this->module->instructions.push(inst);
  return id;
}

RIRValueId RIRBuilder::buildDeclare(RIRValue inst) {
  RIRValueId id = this->build(inst);
  this->module->roots.push(id);
  return id;
}

RIRValueId RIRBuilder::buildInst(RIRValue inst) {
  RIRValueId id = this->build(inst);
  this->block.get()->instructions.push(id);
  return id;
}

RIRValueId RIRBuilder::buildLocalVariable(RIRTypeId type, RIRTypeId result) {
  RIRValue inst = {.kind = RIRValueKind::LocalVariable, .result = result};
  inst.local = {.type = type};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildLoad(RIRValueId ptr, RIRTypeId result) {
  RIRValue inst = {.kind = RIRValueKind::Load, .result = result};
  inst.load = {.ptr = ptr};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildStore(RIRValueId ptr, RIRValueId value,
                                  RIRTypeId result) {
  RIRValue inst = {.kind = RIRValueKind::Store, .result = result};
  inst.store = {.ptr = ptr, .value = value};
  return this->buildInst(inst);
}

void RIRBuilder::addDebugLocation(RIRValueId inst, SrcLoc location) {
  std::cerr << "TODO: Implement debug information. This is not a crash\n";
  // TODO: Implement debug information
}

void RIRBuilder::addDebugName(RIRValueId inst, String name) {
  std::cerr << "TODO: Implement debug information. This is not a crash\n";
  // TODO: Implement debug information
}
