#include "uir.hpp"
#include "allocator.hpp"
#include <cstdlib>
#include <iostream>

void UIRContext::init(Allocator *allocator) {
  this->allocator = allocator;
  this->arena.init(allocator, 1024 * 1024 * 8);
}

void UIRContext::deinit() { this->arena.deinit(); }

void UIRModule::init(Allocator *allocator, Allocator *arena_allocator) {
  this->allocator = allocator;

  this->instructions.init(allocator, arena_allocator, 1024 * 1024 * 8);
  this->blocks.init(allocator, arena_allocator, 1024 * 1024);
  this->scopes.init(allocator, arena_allocator, 1024 * 1024);

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

UIRValue *UIRContext::make(UIRValue value) {
  UIRValue *ptr = (UIRValue *)this->arena.alloc(sizeof(UIRValue));
  *ptr = value;
  return ptr;
}

UIRValue *UIRContext::makeLiteral(UIRLiteral lit) {
  UIRValue inst = {.kind = UIRValueKind::Literal};
  inst.literal = lit;
  return this->make(inst);
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
