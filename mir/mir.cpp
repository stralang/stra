#include "mir.hpp"
#include "allocator.hpp"
#include <cstdlib>
#include <iostream>

void MIRContext::init(Allocator *allocator) {
  this->allocator = allocator;
  this->arena.init(allocator, 1024 * 1024 * 8);
}

void MIRContext::deinit() { this->arena.deinit(); }

void MIRModule::init(Allocator *allocator) {
  this->arena.init(allocator, 1024 * 1024 * 8);
  this->definitions = (MIRScope *)this->arena.alloc(sizeof(MIRScope));
  this->definitions->list.init(allocator, 32);
  this->definitions->owner = nullptr;
}

void MIRModule::deinit() {
  this->arena.deinit();
  this->definitions->list.deinit();
}

MIRValue *MIRContext::make(MIRValue value) {
  MIRValue *ptr = (MIRValue *)this->arena.alloc(sizeof(MIRValue));
  *ptr = value;
  return ptr;
}

MIRValue *MIRContext::makeLiteral(MIRLiteral lit) {
  MIRValue inst = {.kind = MIRValueKind::Literal};
  inst.literal = lit;
  return this->make(inst);
}

bool MIRBlock::hasTerminator() {
  size_t i = this->instructions.length;
  while (i > 0) {
    i -= 1;
    MIRValue *inst = this->instructions.getUnchecked(i);
    if (inst->kind == MIRValueKind::Return) {
      return true;
    }
  }

  return false;
}
