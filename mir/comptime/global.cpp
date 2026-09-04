#include "comptime/comptime.hpp"
#include "define.hpp"
#include "literal.hpp"

Type *placeholderType(MIRModule *module, MIRValue *inst) {
  Type raw_type;
  if (inst->kind == MIRValueKind::Struct) {
    raw_type.kind = TypeKind::Struct;
    raw_type._struct.inst = inst;
  } else if (inst->kind == MIRValueKind::Enum) {
    raw_type.kind = TypeKind::Enum;
    raw_type._enum.inst = inst;
  } else if (inst->kind == MIRValueKind::Union) {
    raw_type.kind = TypeKind::Union;
    raw_type._union.inst = inst;
  } else if (inst->kind == MIRValueKind::Namespace) {
    raw_type.kind = TypeKind::Namespace;
    raw_type._namespace.inst = inst;
  }

  return module->ctx->type_cache->get(raw_type);
}

MIRLiteral executeGlobalVariable(MIRComptime *state, MIRModule *module,
                                 ComptimeStackFrame *frame, MIRValue *inst) {
  // Type
  if (inst->global_variable.type.isSome()) {
    // // Placeholder Type for cyclic dependencies
    // Type *placeholder = placeholderType(module, inst->global_variable.type);
    // if (placeholder != nullptr) {
    //   inst->result_type = module->ctx->type_cache->get({
    //       .kind = TypeKind::Pointer,
    //       .child = placeholder,
    //       .is_constant = true,
    //   });
    // }

    // Get Type
    MIRValue *type_inst = module->getInstr(inst->global_variable.type.get());
    MIRLiteral type_literal = state->execute(module, type_inst);
    // TODO:
    // expect(type_literal.lit_type->kind == TypeKind::TypeId,
    //        inst->global_variable.type->source_location,
    //        "Field type must be a typeid");

    inst->result_type = module->ctx->type_cache->get({
        .kind = TypeKind::Pointer,
        .child = type_literal._typeid,
        .is_constant = false,
    });
  }

  // Analyse Constant
  if (inst->global_variable.constant.isSome()) {
    // // Placeholder Type for cyclic dependencies
    // if (inst->global_variable.type.isNone()) {
    //   Type *placeholder =
    //       placeholderType(module, inst->global_variable.constant);
    //   if (placeholder != nullptr) {
    //     inst->result_type = module->ctx->type_cache->get({
    //         .kind = TypeKind::Pointer,
    //         .child = placeholder,
    //         .is_constant = true,
    //     });
    //   }
    // }

    // Get Initial
    MIRValue *constant_inst =
        module->getInstr(inst->global_variable.constant.get());
    MIRLiteral const_literal = state->execute(module, constant_inst);
    Type *type = const_literal.lit_type;
    // expect(type != nullptr, inst->global_variable.constant->source_location,
    //        "Couldn't determine type of constant");

    // Check
    if (inst->global_variable.type.isNone()) {
      inst->result_type = module->ctx->type_cache->get({
          .kind = TypeKind::Pointer,
          .child = type,
          .is_constant = false,
      });
    } else {
      if (type->kind == TypeKind::Integer && type->integer.is_untyped) {
        const_literal.lit_type = inst->result_type->child;
        type = inst->result_type->child;
      }

      // TODO:
      // autoCast(state, inst->global_variable.constant,
      // inst->result_type->child);
      // expect(compareTypes(inst->result_type->child,
      //                     inst->global_variable.constant->result_type),
      //        inst->global_variable.constant->source_location,
      //        "Field initial doesn't match type. Field Type: `"
      //            << inst->result_type << "` Initial Type: `"
      //            << inst->global_variable.constant->result_type << "`\n");
    }

    // Set data
    MIRLiteral *data_literal =
        (MIRLiteral *)state->arena->alloc(sizeof(MIRLiteral));
    *data_literal = const_literal;

    MIRLiteral *ptr_literal =
        (MIRLiteral *)state->arena->alloc(sizeof(MIRLiteral));
    *ptr_literal = {
        .lit_type = inst->result_type,
        .kind = MIRLiteralKind::Typed,
        .pointer = data_literal,
    };

    state->globals.insert(inst, ptr_literal);
    return *ptr_literal;
  }

  // TODO: Default values
  return {.lit_type = nullptr};
}
