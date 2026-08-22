#include "analysis.hpp"
#include "define.hpp"
#include "literal.hpp"
#include "mir.hpp"

// --- Forward Declarations ---
void analyseBlock(MIRAnalyser *analyser, MIRModule *module, MIRBlock *block);
void analyseScope(MIRAnalyser *analyser, MIRModule *module, MIRScope *scope);
// --- Forward Declarations ---

bool compareTypes(Type *lhs, Type *rhs) {
  // TODO:
  return lhs->hashcode == rhs->hashcode;
}

void analyseBinary(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst) {
  analyse(analyser, module, inst->binop.lhs);
  analyse(analyser, module, inst->binop.rhs);

  Type *lhs_primitive = inst->binop.lhs->result_type;
  Type *rhs_primitive = inst->binop.rhs->result_type;

  switch (inst->binop.opcode) {
  case MIROpcode::Add:
  case MIROpcode::Sub:
  case MIROpcode::Mul:
  case MIROpcode::Div:
  case MIROpcode::Mod: {
    if (lhs_primitive->kind != TypeKind::Integer &&
        lhs_primitive->kind != TypeKind::Float &&
        lhs_primitive->kind != TypeKind::Pointer) {
      expect(false, TODO_SRCLOC,
             "LHS must be of Integer, Float, Pointer, or SIMD."); // TODO:
    }

    expect(compareTypes(lhs_primitive, rhs_primitive), TODO_SRCLOC,
           "LHS cannot operate with RHS"); // TODO:

    inst->result_type = lhs_primitive;
    break;
  }
  case MIROpcode::Or:
  case MIROpcode::Xor:
  case MIROpcode::And:
  case MIROpcode::LeftShift:
  case MIROpcode::RightShift: {
    expect(lhs_primitive->kind == TypeKind::Integer, TODO_SRCLOC,
           "LHS must be an Integer. Got `" << lhs_primitive << "`");
    expect(compareTypes(lhs_primitive, rhs_primitive), TODO_SRCLOC,
           "LHS `" << lhs_primitive << "` cannot operate with RHS `"
                   << rhs_primitive << "`");

    inst->result_type = lhs_primitive;
    break;
  }
  case MIROpcode::EqualTo:
  case MIROpcode::NotEqualTo: {
    expect(compareTypes(lhs_primitive, rhs_primitive), TODO_SRCLOC,
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
      expect(false, TODO_SRCLOC,
             "LHS must be of Integer, Float, or SIMD. Got `" << lhs_primitive
                                                             << "`");
    }

    expect(compareTypes(lhs_primitive, rhs_primitive), TODO_SRCLOC,
           "LHS `" << lhs_primitive << "` cannot operate with RHS `"
                   << rhs_primitive << "`");
    inst->result_type = module->ctx->type_cache->get({.kind = TypeKind::Bool});
    break;
  }
  case MIROpcode::As:
  case MIROpcode::Bitcast: {
    expect(lhs_primitive->kind != TypeKind::TypeId, TODO_SRCLOC,
           "LHS must not be a type");
    expect(rhs_primitive->kind == TypeKind::TypeId, TODO_SRCLOC,
           "RHS must be a type");

    MIRLiteral rhs_literal =
        analyser->comptime_state.execute(module, inst->binop.rhs);
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

    expect(allowed, TODO_SRCLOC,
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
      expect(false, TODO_SRCLOC,
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
    expect(child_primitive->kind == TypeKind::Bool, TODO_SRCLOC,
           "Child must be Bool. Got `" << child_primitive << "`");
    inst->result_type = child_primitive;
    break;
  }
  case MIROpcode::BitwiseNot: {
    expect(child_primitive->kind == TypeKind::Integer, TODO_SRCLOC,
           "Child must be Integer. Got `" << child_primitive << "`");
    inst->result_type = child_primitive;
    break;
  }
  }
}

void analyse(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst) {
  switch (inst->kind) {
  case MIRValueKind::Alloca: {
    // Analyse Type
    if (inst->alloca.type != nullptr) {
      analyse(analyser, module, inst->alloca.type);
      Type *type = inst->alloca.type->result_type;
      expect(type->kind == TypeKind::TypeId, TODO_SRCLOC,
             "Field type must be a typeid"); // TODO:

      inst->result_type = module->ctx->type_cache->get({
          .kind = TypeKind::Pointer,
          .child = type,
          .is_constant = false,
      });
    }

    // Analyse Initial
    if (inst->alloca.initial != nullptr) {
      analyse(analyser, module, inst->alloca.initial);
      Type *type = inst->alloca.initial->result_type;

      if (inst->alloca.type == nullptr) {
        inst->result_type = module->ctx->type_cache->get({
            .kind = TypeKind::Pointer,
            .child = type,
            .is_constant = false,
        });
      } else {
        // TODO: Auto Cast and Compare
      }
    }
    break;
  }
  case MIRValueKind::Load: {
    analyse(analyser, module, inst->load.ptr);
    expect(inst->load.ptr->result_type->kind == TypeKind::Pointer, TODO_SRCLOC,
           "Can only load from pointer"); // TODO:
    inst->result_type = inst->load.ptr->result_type->child;
    break;
  }
  case MIRValueKind::Store: {
    analyse(analyser, module, inst->store.ptr);
    analyse(analyser, module, inst->store.value);
    expect(inst->store.ptr->result_type->child ==
               inst->store.value->result_type,
           TODO_SRCLOC,
           "Cannot store non-matching type"); // TODO:
    inst->result_type = nullptr;
    break;
  }
  case MIRValueKind::Arg: {
    analyse(analyser, module, inst->arg.type);
    inst->result_type = inst->arg.type->result_type;
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

  case MIRValueKind::GlobalVariable: {
    // Analyse Type
    if (inst->global_variable.type != nullptr) {
      analyse(analyser, module, inst->global_variable.type);
      Type *type = inst->global_variable.type->result_type;
      expect(type->kind == TypeKind::TypeId, TODO_SRCLOC,
             "Field type must be a typeid"); // TODO:

      inst->result_type = module->ctx->type_cache->get({
          .kind = TypeKind::Pointer,
          .child = type,
          .is_constant = false,
      });
    }

    // Analyse Constant
    if (inst->global_variable.constant != nullptr) {
      analyse(analyser, module, inst->global_variable.constant);
      Type *type = inst->global_variable.constant->result_type;
      expect(type != nullptr, TODO_SRCLOC,
             "Couldn't determine type of constant");

      if (inst->global_variable.type == nullptr) {
        inst->result_type = module->ctx->type_cache->get({
            .kind = TypeKind::Pointer,
            .child = type,
            .is_constant = false,
        });
      } else {
        // TODO: Auto Cast and Compare
      }
    }
    break;
  }
  case MIRValueKind::Function: {
    // Analyse Type
    Type fn_type = {.kind = TypeKind::Function, .is_constant = true};
    fn_type.function.arguments = {
        .ptr = (Type **)analyser->arena.alloc(
            sizeof(MIRValue) * inst->function.parameter_types.len),
        .len = inst->function.parameter_types.len,
    };

    // Analyse Parameters
    for (size_t i = 0; i < inst->function.parameter_types.len; i++) {
      MIRValue *param = inst->function.parameter_types.ptr[i];
      analyse(analyser, module, param);

      MIRLiteral param_literal =
          analyser->comptime_state.execute(module, param);
      fn_type.function.arguments.ptr[i] = param_literal._typeid;
    }

    // Analyse Return Type
    analyse(analyser, module, inst->function.return_type);
    MIRLiteral return_literal =
        analyser->comptime_state.execute(module, inst->function.return_type);
    fn_type.function.return_type = return_literal._typeid;

    inst->result_type = module->ctx->type_cache->get(fn_type);

    // Analyse Body
    if (inst->function.globals != nullptr) {
      analyseScope(analyser, module, inst->function.globals);
      for (size_t i = 0; i < inst->function.blocks.length; i++) {
        analyseBlock(analyser, module, inst->function.blocks.getUnchecked(i));
      }
    } else if (!inst->function.undefined) {
      inst->result_type =
          module->ctx->type_cache->get({.kind = TypeKind::TypeId});
    }
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
}

void MIRAnalyser::deinit() { this->arena.deinit(); }
