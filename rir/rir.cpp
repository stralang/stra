#include "rir.hpp"
#include "rir/type.hpp"

void RIRModule::init(Allocator *allocator, Allocator *arena_allocator) {
  this->allocator = allocator;
  this->instructions.init(allocator, arena_allocator, sizeof(RIRValue) * 32767);
  this->blocks.init(allocator, arena_allocator, sizeof(RIRBlock) * 512);
}

void RIRModule::deinit() {}

void RIRContext::init(Allocator *allocator, Allocator *arena_allocator) {
  this->allocator = allocator;
  this->types.init(allocator, arena_allocator, sizeof(RIRType) * 256);
  this->modules.init(allocator, arena_allocator, sizeof(RIRModule) * 32);
}

void RIRContext::deinit() {
  this->modules.deinit();
  this->types.deinit();
}

RIRValue *RIRContext::getInst(RIRValueId id) {
  return this->modules.getPtr(id.module)->instructions.getPtr(id.local);
}

RIRBlock *RIRContext::getBlock(RIRBlockId id) {
  return this->modules.getPtr(id.module)->blocks.getPtr(id.local);
}

RIRType *RIRContext::getType(RIRTypeId id) { return this->types.getPtr(id); }
