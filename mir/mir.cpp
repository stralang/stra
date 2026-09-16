#include "mir.hpp"
#include "allocator.hpp"
#include <cstdlib>

void MIRContext::init(Allocator *allocator, Allocator *arena_allocator) {
  this->allocator = allocator;
  this->modules.init(allocator, arena_allocator, sizeof(MIRModule) * 32);
}

void MIRContext::deinit() { this->modules.deinit(); }

void MIRModule::init(Allocator *allocator, Allocator *arena_allocator) {
  this->allocator = allocator;

  this->instructions.init(allocator, arena_allocator, sizeof(MIRValue) * 65535);
  this->blocks.init(allocator, arena_allocator, sizeof(MIRBlock) * 1024);
  this->scopes.init(allocator, arena_allocator, sizeof(MIRScope) * 256);

  this->scopes.push({});

  this->definitions = this->scopes.getPtrUnchecked(0);
  this->definitions->list.init(allocator, 32);
  this->definitions->owner = nullptr;
}

void MIRModule::deinit() {
  this->instructions.deinit();
  this->blocks.deinit();
  this->scopes.deinit();
  this->definitions->list.deinit();
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
