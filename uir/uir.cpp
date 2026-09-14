#include "uir.hpp"
#include "allocator.hpp"
#include <cstdlib>
#include <iostream>

void UIRContext::init(Allocator *allocator, Allocator *arena_allocator) {
  this->allocator = allocator;
  this->modules.init(allocator, arena_allocator, sizeof(UIRModule) * 32);
}

void UIRContext::deinit() { this->modules.deinit(); }

void UIRModule::init(Allocator *allocator, Allocator *arena_allocator) {
  this->allocator = allocator;

  this->instructions.init(allocator, arena_allocator, sizeof(UIRValue) * 65535);
  this->blocks.init(allocator, arena_allocator, sizeof(UIRBlock) * 1024);
  this->scopes.init(allocator, arena_allocator, sizeof(UIRScope) * 256);

  this->scopes.push({});

  this->definitions = this->scopes.getPtrUnchecked(0);
  this->definitions->list.init(allocator, 32);
  this->definitions->owner = nullptr;
}

void UIRModule::deinit() {
  this->instructions.deinit();
  this->blocks.deinit();
  this->scopes.deinit();
  this->definitions->list.deinit();
}

bool UIRBlock::hasTerminator() {
  size_t i = this->instructions.length;
  while (i > 0) {
    i -= 1;
    UIRValue *inst = this->instructions.getUnchecked(i);
    if (inst->kind == UIRValueKind::Return) {
      return true;
    }
  }

  return false;
}
