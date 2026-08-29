#pragma once

#include "allocator.hpp"
#include "containers.hpp"
#include "literal.hpp"
#include "mir.hpp"

struct ComptimeStackFrame {
  HashMap<MIRValue *, size_t> lookup;
  ArrayList<MIRLiteral> values;

  MIRLiteral get(MIRValue *from) {
    if (from->kind == MIRValueKind::Literal) {
      return from->literal;
    }

    size_t idx = *this->lookup.get(from);
    return this->values.getUnchecked(idx);
  }
};

struct MIRComptime {
  ArrayList<ComptimeStackFrame> call_stack;

  DynamicArena *arena;
  Allocator *allocator;

  MIRLiteral execute(MIRModule *module, MIRValue *inst);

  void init(Allocator *allocator, DynamicArena *arena);
  void deinit();

  void pushStack();
  void popStack();
  ComptimeStackFrame *currentStack();
};
