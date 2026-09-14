#pragma once

#include "allocator.hpp"
#include "containers.hpp"
#include "literal.hpp"
#include "uir.hpp"

struct UIRAnalyser; // Forward Declaration
struct UIRComptime; // Forward Declaration

struct ComptimeStackFrame {
  HashMap<UIRValue *, size_t> lookup;
  ArrayList<UIRLiteral *> values;
  DynamicArena arena;
  size_t arg_count = 0;

  UIRLiteral *inject(UIRLiteral lit) {
    UIRLiteral *ptr = (UIRLiteral *)this->arena.alloc(sizeof(UIRLiteral));
    *ptr = lit;
    this->values.push(ptr);
    return ptr;
  }

  UIRLiteral *add(UIRValue *inst) {
    if (inst != nullptr) {
      // Second allocation check
      size_t *cache = this->lookup.get(inst);
      if (cache != nullptr) {
        return this->values.getUnchecked(*cache);
      }

      // Insert
      this->lookup.insert(inst, this->values.length);
    }

    UIRLiteral *lit = (UIRLiteral *)this->arena.alloc(sizeof(UIRLiteral));
    this->values.push(lit);
    return lit;
  }
};

struct UIRComptime {
  ArrayList<ComptimeStackFrame> call_stack;

  DynamicArena *arena;
  Allocator *allocator;
  UIRAnalyser *analyser;
  UIRContext *ctx;

  UIRLiteral execute(UIRModule *module, UIRValue *inst);

  void init(Allocator *allocator, DynamicArena *arena);
  void deinit();

  void pushStack();
  void popStack();
  ComptimeStackFrame *currentStack();

  UIRLiteral getValue(ComptimeStackFrame *frame, UIRModule *module,
                      UIRValue *from);
};
