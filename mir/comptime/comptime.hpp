#pragma once

#include "allocator.hpp"
#include "containers.hpp"
#include "literal.hpp"
#include "mir.hpp"

struct MIRAnalyser; // Forward Declaration
struct MIRComptime; // Forward Declaration

struct ComptimeStackFrame {
  HashMap<MIRValue *, size_t> lookup;
  ArrayList<MIRLiteral *> values;
  DynamicArena arena;
  size_t arg_count = 0;

  MIRLiteral *add(MIRValue *inst) {
    if (inst != nullptr) {
      this->lookup.insert(inst, this->values.length);
    }

    MIRLiteral *lit = (MIRLiteral *)this->arena.alloc(sizeof(MIRLiteral));
    this->values.push(lit);
    return lit;
  }
};

struct MIRComptime {
  ArrayList<ComptimeStackFrame> call_stack;
  HashMap<MIRValue *, MIRLiteral *> globals;

  DynamicArena *arena;
  Allocator *allocator;
  MIRAnalyser *analyser;

  MIRLiteral execute(MIRModule *module, MIRValue *inst);

  void init(Allocator *allocator, DynamicArena *arena);
  void deinit();

  void pushStack();
  void popStack();
  ComptimeStackFrame *currentStack();

  MIRLiteral *getValue(ComptimeStackFrame *frame, MIRModule *module,
                       MIRValueId from);

  void foldGlobals();
};
