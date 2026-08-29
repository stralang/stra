#include "analysis.hpp"
#include "define.hpp"
#include "literal.hpp"
#include "mir.hpp"

// --- Forward Declarations ---
void analyseBlock(MIRAnalyser *analyser, MIRModule *module, MIRBlock *block);
void analyseScope(MIRAnalyser *analyser, MIRModule *module, MIRScope *scope);
// --- Forward Declarations ---

void analyseBinary(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst) {
  MIRValue *lhs = inst->binop.lhs;
  MIRValue *rhs = inst->binop.rhs;
  analyse(analyser, module, lhs);
  analyse(analyser, module, rhs);

  // Convert from untyped
  if (lhs->result_type->kind == rhs->result_type->kind) {
    bool lhs_untyped = false;
    bool rhs_untyped = false;
    if (lhs->result_type->kind == TypeKind::Integer) {
      lhs_untyped = lhs->result_type->integer.is_untyped;
    } else if (lhs->result_type->kind == TypeKind::Float) {
      lhs_untyped = lhs->result_type->_float.is_untyped;
    }
    if (rhs->result_type->kind == TypeKind::Integer) {
      rhs_untyped = rhs->result_type->integer.is_untyped;
    } else if (rhs->result_type->kind == TypeKind::Float) {
      rhs_untyped = rhs->result_type->_float.is_untyped;
    }

    if (lhs_untyped && !rhs_untyped) {
      fixUntyped(analyser, lhs, rhs->result_type);
    } else if (!lhs_untyped && rhs_untyped) {
      fixUntyped(analyser, rhs, lhs->result_type);
    }
  }

  // Get primitive type
  Type *lhs_primitive = lhs->result_type;
  Type *rhs_primitive = rhs->result_type;

  switch (inst->binop.opcode) {
  case MIROpcode::Add:
  case MIROpcode::Sub:
  case MIROpcode::Mul:
  case MIROpcode::Div:
  case MIROpcode::Mod: {
    if (lhs_primitive->kind != TypeKind::Integer &&
        lhs_primitive->kind != TypeKind::Float &&
        lhs_primitive->kind != TypeKind::Pointer) {
      expect(false, lhs->source_location,
             "LHS must be of Integer, Float, Pointer, or SIMD.");
    }

    expect(compareTypes(lhs_primitive, rhs_primitive), rhs->source_location,
           "LHS cannot operate with RHS");

    inst->result_type = lhs_primitive;
    break;
  }
  case MIROpcode::Or:
  case MIROpcode::Xor:
  case MIROpcode::And:
  case MIROpcode::LeftShift:
  case MIROpcode::RightShift: {
    expect(lhs_primitive->kind == TypeKind::Integer, lhs->source_location,
           "LHS must be an Integer. Got `" << lhs_primitive << "`");
    expect(compareTypes(lhs_primitive, rhs_primitive), rhs->source_location,
           "LHS `" << lhs_primitive << "` cannot operate with RHS `"
                   << rhs_primitive << "`");

    inst->result_type = lhs_primitive;
    break;
  }
  case MIROpcode::EqualTo:
  case MIROpcode::NotEqualTo: {
    expect(compareTypes(lhs_primitive, rhs_primitive), rhs->source_location,
           "LHS `" << lhs_primitive << "` cannot operate with RHS `"
                   << rhs_primitive << "`");

    inst->result_type = module->ctx->type_cache->get({.kind = TypeKind::Bool});
    break;
  }
  case MIROpcode::LessThen:
  case MIROpcode::GreaterThen:
  case MIROpcode::LessThenOrEqualTo:
  case MIROpcode::GreaterThenOrEqualTo: {
    if (lhs_primitive->kind != TypeKind::Integer &&
        lhs_primitive->kind != TypeKind::Float) {
      expect(false, lhs->source_location,
             "LHS must be of Integer, Float, or SIMD. Got `" << lhs_primitive
                                                             << "`");
    }

    expect(compareTypes(lhs_primitive, rhs_primitive), rhs->source_location,
           "LHS `" << lhs_primitive << "` cannot operate with RHS `"
                   << rhs_primitive << "`");
    inst->result_type = module->ctx->type_cache->get({.kind = TypeKind::Bool});
    break;
  }
  case MIROpcode::As:
  case MIROpcode::Bitcast: {
    expect(lhs_primitive->kind != TypeKind::TypeId, lhs->source_location,
           "LHS must not be a type");
    expect(rhs_primitive->kind == TypeKind::TypeId, rhs->source_location,
           "RHS must be a type");

    MIRLiteral rhs_literal = analyser->comptime_state.execute(module, rhs);
    inst->result_type = rhs_literal._typeid;

    // `As` cast restrictions
    if (inst->binop.opcode == MIROpcode::Bitcast) {
      break;
    }

    Type *src_type = lhs_primitive;
    Type *dst_type = rhs_literal._typeid;
    bool allowed = false;

    if (src_type->kind == TypeKind::Bool) {
      allowed = (dst_type->kind == TypeKind::Bool ||
                 dst_type->kind == TypeKind::Integer ||
                 dst_type->kind == TypeKind::Float);
    } else if (src_type->kind == TypeKind::Integer) {
      allowed = dst_type->kind == TypeKind::Integer ||
                dst_type->kind == TypeKind::Float ||
                dst_type->kind == TypeKind::Pointer;
    } else if (src_type->kind == TypeKind::Float) {
      allowed = dst_type->kind == TypeKind::Integer ||
                dst_type->kind == TypeKind::Float;
    } else if (src_type->kind == TypeKind::Pointer) {
      allowed = dst_type->kind == TypeKind::Integer &&
                !dst_type->integer.is_untyped && !dst_type->integer.is_signed &&
                dst_type->integer.bits == -1;
    } else if (src_type->kind == TypeKind::Slice) {
      if (src_type->slice.length == 0 && dst_type->slice.length == 0) {
        allowed = true; // No-Op
      } else if (src_type->slice.length > 0 && dst_type->slice.length == 0) {
        allowed = true; // Compile-time to Runtime
      }

      allowed &= compareTypes(src_type->slice.type, dst_type->slice.type);
    } else if (src_type->kind == TypeKind::Enum) {
      allowed = dst_type->kind == TypeKind::Integer;
    }

    expect(allowed, inst->source_location,
           "Cannot `as` cast `" << src_type << "` to `" << dst_type << "`");
    break;
  }
  }
}

void analyseUnary(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst) {
  analyse(analyser, module, inst->unaryop.value);

  Type *child_primitive = inst->unaryop.value->result_type;

  switch (inst->unaryop.opcode) {
  case MIROpcode::Minus: {
    if (child_primitive->kind != TypeKind::Integer &&
        child_primitive->kind != TypeKind::Float) {
      expect(false, inst->unaryop.value->source_location,
             "Child must be of Integer, Float, or SIMD. Got `"
                 << child_primitive << "`");
    }

    if (child_primitive->kind == TypeKind::Integer &&
        !child_primitive->integer.is_signed) {
      Type ty = *child_primitive;
      ty.integer.is_signed = true;
      inst->result_type = module->ctx->type_cache->get(ty);
    } else {
      inst->result_type = child_primitive;
    }
    break;
  }
  case MIROpcode::LogicalNot: {
    expect(child_primitive->kind == TypeKind::Bool,
           inst->unaryop.value->source_location,
           "Child must be Bool. Got `" << child_primitive << "`");
    inst->result_type = child_primitive;
    break;
  }
  case MIROpcode::BitwiseNot: {
    expect(child_primitive->kind == TypeKind::Integer,
           inst->unaryop.value->source_location,
           "Child must be Integer. Got `" << child_primitive << "`");
    inst->result_type = child_primitive;
    break;
  }
  }
}

void analyse(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst) {
  switch (inst->kind) {
  case MIRValueKind::Alloca: {
    MIRLiteral type_literal =
        analyser->comptime_state.execute(module, inst->alloca.type);
    expect(type_literal.lit_type->kind == TypeKind::TypeId,
           inst->alloca.type->source_location, "Field type must be a typeid");

    inst->result_type = module->ctx->type_cache->get({
        .kind = TypeKind::Pointer,
        .child = type_literal._typeid,
        .is_constant = false,
    });
    break;
  }
  case MIRValueKind::Load: {
    analyse(analyser, module, inst->load.ptr);
    expect(inst->load.ptr->result_type->kind == TypeKind::Pointer,
           inst->load.ptr->source_location, "!MIR! Can only load from pointer");
    inst->result_type = inst->load.ptr->result_type->child;
    break;
  }
  case MIRValueKind::Store: {
    analyse(analyser, module, inst->store.ptr);
    analyse(analyser, module, inst->store.value);
    autoCast(analyser, inst->store.value, inst->store.ptr->result_type->child);
    expect(inst->store.ptr->result_type->child ==
               inst->store.value->result_type,
           inst->source_location, "Cannot assign non-matching types");
    inst->result_type = nullptr;
    break;
  }
  case MIRValueKind::Arg: {
    MIRLiteral arg_literal =
        analyser->comptime_state.execute(module, inst->arg.type);

    inst->result_type = module->ctx->type_cache->get({
        .kind = TypeKind::Pointer,
        .child = arg_literal._typeid,
        .is_constant = true,
    });
    break;
  }
  case MIRValueKind::BinOp: {
    analyseBinary(analyser, module, inst);
    break;
  }
  case MIRValueKind::UnaryOp: {
    analyseUnary(analyser, module, inst);
    break;
  }
  case MIRValueKind::Call: {
    MIRValue *callee = inst->call.callee;
    analyse(analyser, module, callee);

    // Auto dereference
    Type *callee_type = callee->result_type;
    if (callee_type->kind == TypeKind::Pointer) {
      callee_type = callee_type->child;
    }

    // Get function
    expect(callee_type->kind == TypeKind::Function, callee->source_location,
           "Callee must be a function. Got " << callee_type << "`");

    Type *fn_type = callee_type;

    // Get receiver
    size_t initial_idx = 0;
    if (inst->call.receiver != nullptr) {
      analyse(analyser, module, inst->call.receiver);

      if (inst->call.receiver->result_type->kind != TypeKind::TypeId) {
        expect(fn_type->function.arguments.len >= 1,
               inst->call.receiver->source_location,
               "Receiver expects method with atleast 1 argument");

        Type *expected_type = fn_type->function.arguments.ptr[0];
        Type *receiver_type = inst->call.receiver->result_type;
        expect(compareTypes(expected_type, receiver_type),
               inst->call.receiver->source_location,
               "Receiver `" << receiver_type << "` doesn't match `"
                            << expected_type << "`");

        initial_idx = 1;
      } else {
        inst->call.receiver = nullptr;
      }
    }

    // Analyse arguments
    for (size_t i = 0; i < inst->call.arguments.len; i++) {
      MIRValue *arg = inst->call.arguments.ptr[i];
      if (i > fn_type->function.arguments.len - initial_idx) {
        expect(false, arg->source_location, "Too many arguments");
        break;
      }

      Type *expected_type = fn_type->function.arguments.ptr[i + initial_idx];
      analyse(analyser, module, arg);
      autoCast(analyser, arg, expected_type);

      expect(compareTypes(expected_type, arg->result_type),
             arg->source_location,
             "Argument `" << arg->result_type << "` doesn't match expected `"
                          << expected_type << "`");
    }

    inst->result_type = fn_type->function.return_type;
    break;
  }
  case MIRValueKind::Return: {
    if (inst->parent->parent->kind == MIRValueKind::Function) {
      MIRValue *function = inst->parent->parent;
      Type *expected_type = function->result_type->function.return_type;

      if (inst->ret.value == nullptr) {
        expect(expected_type->kind == TypeKind::Void, inst->source_location,
               "Function expects return value");
      } else {
        analyse(analyser, module, inst->ret.value);
        autoCast(analyser, inst->ret.value, expected_type);

        expect(compareTypes(expected_type, inst->ret.value->result_type),
               inst->ret.value->source_location,
               "Unexpected return type. Got `" << inst->ret.value->result_type
                                               << "` Expected `"
                                               << expected_type << "`");
      }
    } else if (inst->ret.value != nullptr) {
      analyse(analyser, module, inst->ret.value);
    }

    inst->result_type = nullptr;
    break;
  }
  case MIRValueKind::Branch: {
    inst->result_type = nullptr;
    break;
  }
  case MIRValueKind::CondBranch: {
    analyse(analyser, module, inst->condbr.condition);
    expect(inst->condbr.condition->result_type->kind == TypeKind::Bool,
           inst->source_location, "Conditional must be Bool")
        inst->result_type = nullptr;
    break;
  }
  case MIRValueKind::Switch: {
    analyse(analyser, module, inst->_switch.condition);

    for (size_t i = 0; i < inst->_switch.onvals.len; i++) {
      MIRValue *on_val = inst->_switch.onvals.ptr[i];
      analyse(analyser, module, on_val);

      MIRLiteral onval_literal =
          analyser->comptime_state.execute(module, on_val);
      on_val->kind = MIRValueKind::Literal;
      on_val->literal = onval_literal;
    }

    inst->result_type = nullptr;
    break;
  }

  case MIRValueKind::Comptime: {
    inst->literal = analyser->comptime_state.execute(module, inst);
    inst->kind = MIRValueKind::Literal;
    inst->result_type = inst->literal.lit_type;
    break;
  }
  case MIRValueKind::TypeOf: {
    inst->literal = analyser->comptime_state.execute(module, inst);
    inst->kind = MIRValueKind::Literal;
    inst->result_type = inst->literal.lit_type;
    break;
  }

  case MIRValueKind::GlobalVariable: {
    // Analyse Type
    if (inst->global_variable.type != nullptr) {
      MIRLiteral type_literal =
          analyser->comptime_state.execute(module, inst->global_variable.type);
      expect(type_literal.lit_type->kind == TypeKind::TypeId,
             inst->global_variable.type->source_location,
             "Field type must be a typeid");

      inst->result_type = module->ctx->type_cache->get({
          .kind = TypeKind::Pointer,
          .child = type_literal._typeid,
          .is_constant = false,
      });
    }

    // Analyse Constant
    if (inst->global_variable.constant != nullptr) {
      MIRLiteral const_literal = analyser->comptime_state.execute(
          module, inst->global_variable.constant);
      Type *type = const_literal.lit_type;
      expect(type != nullptr, inst->global_variable.constant->source_location,
             "Couldn't determine type of constant");

      // Replace with literal
      MIRValue *new_const = (MIRValue *)analyser->arena.alloc(sizeof(MIRValue));
      new_const->source_location =
          inst->global_variable.constant->source_location;
      new_const->kind = MIRValueKind::Literal;
      new_const->literal = const_literal;
      new_const->result_type = const_literal.lit_type;

      inst->global_variable.constant = new_const;

      // Check
      if (inst->global_variable.type == nullptr) {
        inst->result_type = module->ctx->type_cache->get({
            .kind = TypeKind::Pointer,
            .child = type,
            .is_constant = false,
        });
      } else {
        autoCast(analyser, inst->global_variable.constant,
                 inst->result_type->child);
        expect(compareTypes(inst->result_type->child,
                            inst->global_variable.constant->result_type),
               inst->global_variable.constant->source_location,
               "Field initial doesn't match type. Field Type: `"
                   << inst->result_type << "` Initial Type: `"
                   << inst->global_variable.constant->result_type << "`\n");
      }
    }
    break;
  }
  case MIRValueKind::Function: {
    MIRLiteral type = analyser->comptime_state.execute(module, inst);
    inst->result_type = type._typeid;

    // Analyse Body
    if (inst->function.globals != nullptr) {
      analyseScope(analyser, module, inst->function.globals);
      for (size_t i = 0; i < inst->function.blocks.length; i++) {
        analyseBlock(analyser, module, inst->function.blocks.getUnchecked(i));
      }
    }
    break;
  }
  case MIRValueKind::Literal: {
    inst->result_type = inst->literal.lit_type;
    break;
  }
  default: {
    std::cerr << "TODO: Implement analysis of `" << std::hex
              << (uint16_t)inst->kind << "`\n";
    std::abort();
    break;
  }
  }
}

void analyseBlock(MIRAnalyser *analyser, MIRModule *module, MIRBlock *block) {
  for (size_t i = 0; i < block->instructions.length; i++) {
    analyse(analyser, module, block->instructions.getUnchecked(i));
  }
}

void analyseScope(MIRAnalyser *analyser, MIRModule *module, MIRScope *scope) {
  for (size_t i = 0; i < scope->list.length; i++) {
    analyse(analyser, module, scope->list.getUnchecked(i));
  }
}

void MIRAnalyser::analyse(MIRModule *module) {
  analyseScope(this, module, module->definitions);
}

void MIRAnalyser::init(Allocator *allocator) {
  this->allocator = allocator;
  this->arena.init(allocator, 1024 * 1024 * 8);

  this->comptime_state.init(allocator, &this->arena);
}

void MIRAnalyser::deinit() { this->arena.deinit(); }
