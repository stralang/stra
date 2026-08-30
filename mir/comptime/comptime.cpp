#include "comptime.hpp"
#include "containers.hpp"
#include "define.hpp"
#include "literal.hpp"
#include "mir.hpp"
#include <cstdlib>
#include <iostream>

MIRLiteral execute(MIRComptime *state, MIRModule *module, MIRValue *inst) {
  ComptimeStackFrame *frame = state->currentStack();

  switch (inst->kind) {
  case MIRValueKind::Load: {
    MIRLiteral ptr = frame->get(inst->load.ptr);
    frame->lookup.insert(inst, frame->values.length);
    frame->values.push(*ptr.pointer);
    return ptr;
  }
  case MIRValueKind::Arg: {
    MIRLiteral ty_lit = frame->get(inst->load.ptr);
    // TODO: Compare types

    MIRLiteral *arg_value = frame->values.getPtrUnchecked(frame->arg_count);
    MIRLiteral ptr_lit = {
        .lit_type = module->ctx->type_cache->get({
            .kind = TypeKind::Pointer,
            .child = ty_lit._typeid,
            .is_constant = true,
        }),
        .kind = MIRLiteralKind::Typed,
        .pointer = arg_value,
    };

    frame->lookup.insert(inst, frame->values.length);
    frame->values.push(ptr_lit);

    frame->arg_count += 1;
    return ptr_lit;
  }
  case MIRValueKind::Call: {
    state->pushStack();

    // Get Function
    MIRValue *function = nullptr;
    if (inst->call.callee->kind == MIRValueKind::Function) {
      function = inst->call.callee;
    } else {
      MIRLiteral fn = frame->get(inst->call.callee);
      function = fn.function;
    }

    // Inject Arguments
    for (size_t i = 0; i < inst->call.arguments.len; i++) {
      MIRValue *arg = inst->call.arguments.ptr[i];
      MIRLiteral value = frame->get(arg);
      state->currentStack()->values.push(value);
    }

    executeProgram(state, module, function->function.blocks.get(0));

    MIRLiteral result = *state->currentStack()->values.back();
    state->popStack();
    return result;
  }
  case MIRValueKind::BinOp: {
    return executeBinary(state, module, frame, inst);
  }
  case MIRValueKind::Return: {
    MIRLiteral result;
    if (inst->ret.value != nullptr) {
      result = frame->get(inst->ret.value);
    } else {
      result.lit_type = module->ctx->type_cache->get({.kind = TypeKind::Void});
      result.kind = MIRLiteralKind::Typed;
    }
    return result;
  }

  case MIRValueKind::Comptime: {
    state->pushStack();
    MIRBlock *entry = inst->comptime.blocks.get(0);
    executeProgram(state, module, entry);

    MIRLiteral result = *state->currentStack()->values.back();
    state->popStack();
    return result;
  }
  case MIRValueKind::TypeOf: {
    return MIRLiteral{
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        .kind = MIRLiteralKind::Typed,
        ._typeid = inst->_typeof->result_type,
    };
  }

  case MIRValueKind::Literal: {
    return inst->literal;
  }
  case MIRValueKind::Function: {
    // Analyse Type
    Type raw_type = {.kind = TypeKind::Function, .is_constant = true};
    raw_type.function.arguments = {
        .ptr = (Type **)state->arena->alloc(sizeof(MIRValue) *
                                            inst->function.parameter_types.len),
        .len = inst->function.parameter_types.len,
    };

    // Analyse Parameters
    for (size_t i = 0; i < inst->function.parameter_types.len; i++) {
      MIRValue *param = inst->function.parameter_types.ptr[i];

      MIRLiteral param_literal = execute(state, module, param);
      raw_type.function.arguments.ptr[i] = param_literal._typeid;
    }

    // Analyse Return Type
    MIRLiteral return_literal =
        execute(state, module, inst->function.return_type);
    raw_type.function.return_type = return_literal._typeid;

    // Get final type
    return {
        .lit_type = module->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        ._typeid = module->ctx->type_cache->get(raw_type),
    };
  }
  }

  std::cerr << "TODO: Implement compile-time execution of `" << std::hex
            << (uint16_t)inst->kind << "`\n";
  std::abort();
}

void executeProgram(MIRComptime *state, MIRModule *module,
                    MIRBlock *entrypoint) {
  ComptimeStackFrame *frame = state->currentStack();
  ArrayList<MIRValue *> *program = &entrypoint->instructions;
  size_t pc = 0;
  while (pc < program->length) {
    MIRValue *inst = program->getUnchecked(pc);
    pc += 1;

    // Branch
    if (inst->kind == MIRValueKind::Branch) {
      pc = 0;
      program = &inst->br->instructions;
      continue;
    } else if (inst->kind == MIRValueKind::CondBranch) {
      pc = 0;

      size_t cond_idx = *frame->lookup.get(inst->condbr.condition);
      MIRLiteral cond = frame->values.get(cond_idx);
      assert(cond.lit_type->kind == TypeKind::Bool &&
             "Condition was not boolean");

      if (cond._bool) {
        program = &inst->condbr.then->instructions;
      } else {
        program = &inst->condbr._else->instructions;
      }
      continue;
    }

    // Execute instruction
    frame->lookup.insert(inst, frame->values.length);
    MIRLiteral value = execute(state, module, inst);
    frame->values.push(value);
  }
}

MIRLiteral MIRComptime::execute(MIRModule *module, MIRValue *inst) {
  this->pushStack();
  MIRLiteral result = ::execute(this, module, inst);
  this->popStack();

  return result;
}

void MIRComptime::init(Allocator *allocator, DynamicArena *arena) {
  this->allocator = allocator;
  this->call_stack.init(allocator, 8);
  this->arena = arena;
}
void MIRComptime::deinit() { this->call_stack.deinit(); }

void MIRComptime::pushStack() {
  ComptimeStackFrame frame;
  frame.lookup.init(this->allocator, 32);
  frame.values.init(this->allocator, 32);
  this->call_stack.push(frame);
}

void MIRComptime::popStack() {
  ComptimeStackFrame frame = this->call_stack.pop();
  frame.lookup.deinit();
  frame.values.deinit();
}

ComptimeStackFrame *MIRComptime::currentStack() {
  return this->call_stack.back();
}
