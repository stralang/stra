#include "analysis.hpp"
#include "define.hpp"
#include "literal.hpp"
#include "mir.hpp"

// --- Forward Declarations ---
void analyseBlock(MIRAnalyser *analyser, MIRModule *module, MIRBlock *block);
void analyseScope(MIRAnalyser *analyser, MIRModule *module, MIRScope *scope);
// --- Forward Declarations ---

void analyseBinary(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst) {
  MIRValue *lhs = module->getInstr(inst->binop.lhs);
  MIRValue *rhs = module->getInstr(inst->binop.rhs);
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
  MIRValue *operand = module->getInstr(inst->unaryop.value);
  analyse(analyser, module, operand);

  Type *child_primitive = operand->result_type;

  switch (inst->unaryop.opcode) {
  case MIROpcode::Minus: {
    if (child_primitive->kind != TypeKind::Integer &&
        child_primitive->kind != TypeKind::Float) {
      expect(false, operand->source_location,
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
    expect(child_primitive->kind == TypeKind::Bool, operand->source_location,
           "Child must be Bool. Got `" << child_primitive << "`");
    inst->result_type = child_primitive;
    break;
  }
  case MIROpcode::BitwiseNot: {
    expect(child_primitive->kind == TypeKind::Integer, operand->source_location,
           "Child must be Integer. Got `" << child_primitive << "`");
    inst->result_type = child_primitive;
    break;
  }
  }
}

void analyse(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst) {
  switch (inst->kind) {
  case MIRValueKind::Alloca: {
    MIRValue *type_inst = module->getInstr(inst->alloca.type);
    MIRLiteral type_literal =
        analyser->comptime_state.execute(module, type_inst);
    expect(type_literal.lit_type->kind == TypeKind::TypeId,
           type_inst->source_location, "Field type must be a typeid");

    inst->result_type = module->ctx->type_cache->get({
        .kind = TypeKind::Pointer,
        .child = type_literal._typeid,
        .is_constant = false,
    });
    break;
  }
  case MIRValueKind::Load: {
    MIRValue *ptr_inst = module->getInstr(inst->load.ptr);
    analyse(analyser, module, ptr_inst);
    expect(ptr_inst->result_type->kind == TypeKind::Pointer,
           ptr_inst->source_location, "!MIR! Can only load from pointer");
    inst->result_type = ptr_inst->result_type->child;
    break;
  }
  case MIRValueKind::Store: {
    MIRValue *ptr_inst = module->getInstr(inst->store.ptr);
    MIRValue *value_inst = module->getInstr(inst->store.value);

    analyse(analyser, module, ptr_inst);
    analyse(analyser, module, value_inst);
    autoCast(analyser, value_inst, ptr_inst->result_type->child);
    expect(ptr_inst->result_type->child == value_inst->result_type,
           inst->source_location, "Cannot assign non-matching types");
    inst->result_type = nullptr;
    break;
  }
  case MIRValueKind::Arg: {
    MIRValue *arg_type_inst = module->getInstr(inst->arg.type);
    MIRLiteral arg_literal =
        analyser->comptime_state.execute(module, arg_type_inst);

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
    MIRValue *callee = module->getInstr(inst->call.callee);
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
    if (inst->call.receiver.isSome()) {
      MIRValue *receiver_inst = module->getInstr(inst->call.receiver.get());
      analyse(analyser, module, receiver_inst);

      if (receiver_inst->result_type->kind != TypeKind::TypeId) {
        expect(fn_type->function.arguments.len >= 1,
               receiver_inst->source_location,
               "Receiver expects method with atleast 1 argument");

        Type *expected_type = fn_type->function.arguments.ptr[0];
        Type *receiver_type = receiver_inst->result_type;
        expect(compareTypes(expected_type, receiver_type),
               receiver_inst->source_location,
               "Receiver `" << receiver_type << "` doesn't match `"
                            << expected_type << "`");

        initial_idx = 1;
      } else {
        inst->call.receiver.setNone();
      }
    }

    // Analyse arguments
    for (size_t i = 0; i < inst->call.arguments.len; i++) {
      MIRValue *arg = module->getInstr(inst->call.arguments.ptr[i]);
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
  case MIRValueKind::GEP: {
    // Analyse Pointer
    MIRValue *ptr = module->getInstr(inst->gep.ptr);
    analyse(analyser, module, ptr);
    expect(ptr->result_type->child->kind == TypeKind::Slice,
           inst->source_location, "Cannot index into non-slice");

    inst->result_type = module->ctx->type_cache->get({
        .kind = TypeKind::Pointer,
        .child = ptr->result_type->child->slice.type,
        .is_constant = true,
    });

    // Analyse Index
    Type *usize_ty = module->ctx->type_cache->get({
        .kind = TypeKind::Integer,
        .integer = {.is_untyped = false, .is_signed = false, .bits = -1},
    });

    MIRValue *index = module->getInstr(inst->gep.index);
    analyse(analyser, module, index);
    fixUntyped(analyser, index, usize_ty);
    expect(index->result_type->kind == TypeKind::Integer &&
               !index->result_type->integer.is_signed &&
               index->result_type->integer.bits == -1,
           index->source_location, "Index must be of type `usize`");
    break;
  }
  case MIRValueKind::Return: {
    MIRBlock *parent_block = module->getBlock(inst->parent);
    MIRValue *function = module->getInstr(parent_block->parent);
    if (function->kind == MIRValueKind::Function) {
      Type *expected_type = function->result_type->function.return_type;

      if (inst->ret.value.isNone()) {
        expect(expected_type->kind == TypeKind::Void, inst->source_location,
               "Function expects return value");
      } else {
        MIRValue *value_inst = module->getInstr(inst->ret.value.get());
        analyse(analyser, module, value_inst);
        autoCast(analyser, value_inst, expected_type);

        expect(compareTypes(expected_type, value_inst->result_type),
               value_inst->source_location,
               "Unexpected return type. Got `"
                   << value_inst->result_type << "` Expected `" << expected_type
                   << "`");
      }
    } else if (inst->ret.value.isSome()) {
      MIRValue *value_inst = module->getInstr(inst->ret.value.get());
      analyse(analyser, module, value_inst);
    }

    inst->result_type = nullptr;
    break;
  }
  case MIRValueKind::Branch: {
    inst->result_type = nullptr;
    break;
  }
  case MIRValueKind::CondBranch: {
    MIRValue *cond_inst = module->getInstr(inst->condbr.condition);
    analyse(analyser, module, cond_inst);
    expect(cond_inst->result_type->kind == TypeKind::Bool,
           inst->source_location, "Conditional must be Bool");
    inst->result_type = nullptr;
    break;
  }
  case MIRValueKind::Switch: {
    MIRValue *cond_inst = module->getInstr(inst->_switch.condition);
    analyse(analyser, module, cond_inst);

    for (size_t i = 0; i < inst->_switch.onvals.len; i++) {
      MIRValue *on_val = module->getInstr(inst->_switch.onvals.ptr[i]);
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
    analyser->comptime_state.execute(module, inst);

    if (inst->result_type->child->kind == TypeKind::TypeId) {
      Type *child = inst->result_type->child;
      switch (child->kind) {
      case TypeKind::Struct: {
        MIRScope *scope =
            module->getScope(child->_struct.inst->_struct.definitions);
        analyseScope(analyser, module, scope);
        break;
      }
      case TypeKind::Enum: {
        MIRScope *scope =
            module->getScope(child->_enum.inst->_enum.definitions);
        analyseScope(analyser, module, scope);
        break;
      }
      case TypeKind::Union: {
        MIRScope *scope =
            module->getScope(child->_union.inst->_union.definitions);
        analyseScope(analyser, module, scope);
        break;
      }
      case TypeKind::Namespace: {
        MIRScope *scope =
            module->getScope(child->_namespace.inst->_namespace.definitions);
        analyseScope(analyser, module, scope);
        break;
      }
      }
    }
    break;
  }
  case MIRValueKind::Function: {
    MIRLiteral type = analyser->comptime_state.execute(module, inst);
    inst->result_type = type._typeid;

    // Analyse Body
    MIRScope *global_scope = module->getScope(inst->function.globals);
    analyseScope(analyser, module, global_scope);
    for (size_t i = 0; i < inst->function.blocks.length; i++) {
      MIRBlock *block = module->getBlock(inst->function.blocks.getUnchecked(i));
      analyseBlock(analyser, module, block);
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
    MIRValue *inst = module->getInstr(block->instructions.getUnchecked(i));
    analyse(analyser, module, inst);
  }
}

void analyseScope(MIRAnalyser *analyser, MIRModule *module, MIRScope *scope) {
  for (size_t i = 0; i < scope->list.length; i++) {
    MIRValue *inst = module->getInstr(scope->list.getUnchecked(i));
    analyse(analyser, module, inst);
  }
}

void MIRAnalyser::analyse(MIRModule *module) {
  MIRScope *scope = module->getScope(module->definitions);
  analyseScope(this, module, scope);
}

void MIRAnalyser::init(Allocator *allocator) {
  this->allocator = allocator;
  this->arena.init(allocator, 1024 * 1024 * 8);

  this->comptime_state.init(allocator, &this->arena);
  this->comptime_state.analyser = this;
}

void MIRAnalyser::deinit() { this->arena.deinit(); }
