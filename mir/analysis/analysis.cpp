#include "analysis.hpp"
#include "define.hpp"
#include "literal.hpp"
#include "mir.hpp"

void analyse(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst) {
  switch (inst->kind) {
  case MIRValueKind::LocalVariable: {
    MIRLiteral type_literal =
        analyser->comptime_state.execute(module, inst->local_variable.type);
    expect(type_literal.lit_type->kind == TypeKind::TypeId,
           inst->local_variable.type->source_location,
           "Field type must be a typeid");

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
    if (inst->call.receiver.isSome()) {
      MIRValue *receiver_inst = inst->call.receiver.get();
      analyse(analyser, module, receiver_inst);

      Type *receiver_type = receiver_inst->result_type;
      if (receiver_type->kind != TypeKind::TypeId &&
          (receiver_type->kind != TypeKind::Pointer ||
           receiver_type->child->kind != TypeKind::TypeId)) {
        expect(fn_type->function.arguments.len >= 1,
               receiver_inst->source_location,
               "Receiver expects method with atleast 1 argument");

        Type *expected_type = fn_type->function.arguments.ptr[0];
        expect(compareTypes(expected_type, receiver_type),
               receiver_inst->source_location,
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
  case MIRValueKind::Index: {
    // Analyse Pointer
    analyse(analyser, module, inst->index.ptr);
    MIRValue *ptr = inst->index.ptr;
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

    analyse(analyser, module, inst->index.index);
    MIRValue *index = inst->index.index;
    fixUntyped(analyser, index, usize_ty);
    expect(index->result_type->kind == TypeKind::Integer &&
               !index->result_type->integer.is_signed &&
               index->result_type->integer.bits == -1,
           index->source_location, "Index must be of type `usize`");
    break;
  }
  case MIRValueKind::Range: {
    analyse(analyser, module, inst->range.ptr);
    analyse(analyser, module, inst->range.start);
    analyse(analyser, module, inst->range.end);

    // Check Pointer
    MIRValue *ptr = inst->range.ptr;
    expect(ptr->result_type->child->kind == TypeKind::Slice ||
               ptr->result_type->child->kind == TypeKind::Pointer,
           inst->source_location, "Cannot range into non-slice/pointer");

    // Check Index and Length
    Type *usize_ty = module->ctx->type_cache->get({
        .kind = TypeKind::Integer,
        .integer = {false, false, -1},
    });
    fixUntyped(analyser, inst->range.start, usize_ty);
    fixUntyped(analyser, inst->range.end, usize_ty);

    MIRValue *start = inst->range.start;
    Type *range_type = start->result_type;
    expect(range_type->kind == TypeKind::Integer &&
               !range_type->integer.is_untyped &&
               !range_type->integer.is_signed &&
               range_type->integer.bits == -1 &&
               range_type == inst->range.end->result_type,
           start->source_location, "Range start and end must both be `usize`");

    // Get Result type
    Type ty = {.kind = TypeKind::Slice};
    ty.slice.length = 0;
    if (ptr->result_type->child->kind == TypeKind::Pointer) {
      ty.slice.type = ptr->result_type->child->child;
    } else {
      ty.slice.type = ptr->result_type->child->slice.type;
    }

    inst->result_type = module->ctx->type_cache->get(ty);
    break;
  }
  case MIRValueKind::LookupPtr: {
    analyseLookupPtr(analyser, module, inst);
    break;
  }
  case MIRValueKind::LookupValue: {
    analyseLookupValue(analyser, module, inst);
    break;
  }
  case MIRValueKind::Aggregate: {
    analyseAggregate(analyser, module, inst);
    break;
  }
  case MIRValueKind::Return: {
    if (inst->parent->parent->kind == MIRValueKind::Function) {
      MIRValue *function = inst->parent->parent;
      Type *expected_type = function->result_type->function.return_type;

      if (inst->ret.value.isNone()) {
        expect(expected_type->kind == TypeKind::Void, inst->source_location,
               "Function expects return value");
      } else {
        MIRValue *value_inst = inst->ret.value.get();
        analyse(analyser, module, value_inst);
        autoCast(analyser, value_inst, expected_type);

        expect(compareTypes(expected_type, value_inst->result_type),
               value_inst->source_location,
               "Unexpected return type. Got `"
                   << value_inst->result_type << "` Expected `" << expected_type
                   << "`");
      }
    } else if (inst->ret.value.isSome()) {
      analyse(analyser, module, inst->ret.value.get());
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
           inst->source_location, "Conditional must be Bool");
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
  case MIRValueKind::Alias: {
    analyse(analyser, module, inst->alias);
    inst->result_type = inst->alias->result_type;
    break;
  }

  case MIRValueKind::GlobalVariable:
  case MIRValueKind::Function: {
    analyseGlobal(analyser, module, inst);
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
  this->comptime_state.analyser = this;
}

void MIRAnalyser::deinit() { this->arena.deinit(); }
