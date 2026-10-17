#include "uir.hpp"
#include "allocator.hpp"
#include <cstdlib>

void UIRContext::init(Allocator *allocator, Allocator *arena_allocator) {
  this->allocator = allocator;
  this->modules.init(allocator, arena_allocator, sizeof(UIRModule) * 32);
}

void UIRContext::deinit() {
  for (size_t i = 0; i < this->modules.len(); i++) {
    this->modules.getPtrUnchecked(i)->deinit();
  }

  this->modules.deinit();
}

void UIRModule::init(Allocator *allocator, Allocator *arena_allocator) {
  this->allocator = allocator;

  this->instructions.init(allocator, arena_allocator, sizeof(UIRValue) * 65535);
  this->blocks.init(allocator, arena_allocator, sizeof(UIRBlock) * 1024);
  this->scopes.init(allocator, arena_allocator, sizeof(UIRScope) * 256);

  // Add root scope
  UIRScope root_scope = {
      .id = {.module = this->id, .local = (uint32_t)this->scopes.len()},
  };
  root_scope.list.init(allocator, 32);

  this->definitions = root_scope.id;
  this->scopes.push(root_scope);
}

void UIRModule::deinit() {
  this->instructions.deinit();
  this->blocks.deinit();
  this->scopes.deinit();
}

bool UIRBlock::hasTerminator(UIRContext *ctx) {
  size_t i = this->instructions.length;
  while (i > 0) {
    i -= 1;
    UIRValue *inst = ctx->getInst(this->instructions.getUnchecked(i));
    if (inst->kind == UIRValueKind::Return) {
      return true;
    }
  }

  return false;
}

UIRModule *UIRContext::newModule(String mangling_name) {
  UIRModule mod = {
      .id = (uint32_t)this->modules.len(),
      .mangling_name = mangling_name,
  };
  mod.init(this->allocator, this->allocator);

  this->modules.push(mod);
  return this->modules.back();
}

UIRValue *UIRContext::getInst(UIRValueId id) {
  return this->modules.get(id.module).instructions.getPtr(id.local);
}

UIRBlock *UIRContext::getBlock(UIRBlockId id) {
  return this->modules.get(id.module).blocks.getPtr(id.local);
}

UIRScope *UIRContext::getScope(UIRScopeId id) {
  return this->modules.get(id.module).scopes.getPtr(id.local);
}

RIRType *UIRContext::getType(RIRTypeId id) { return this->types->getPtr(id); }
