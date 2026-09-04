#include "define.hpp"
#include "mir.hpp"

void analyseGlobal(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst) {
  switch (inst->kind) {
  case MIRValueKind::GlobalVariable: {
    // Type
    if (inst->global_variable.type != nullptr) {
      // Get Type
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
      // Get Initial
      MIRLiteral const_literal = analyser->comptime_state.execute(
          module, inst->global_variable.constant);
      Type *type = const_literal.lit_type;
      expect(type != nullptr, inst->global_variable.constant->source_location,
             "Couldn't determine type of constant");

      // Set data
      inst->global_variable.constant->kind = MIRValueKind::Literal;
      inst->global_variable.constant->literal = const_literal;
      inst->global_variable.constant->result_type = const_literal.lit_type;

      // Check
      if (inst->global_variable.type == nullptr) {
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

    // Analyse Definitions
    if (inst->result_type->child->kind == TypeKind::TypeId) {
      Type *child = inst->result_type->child;
      switch (child->kind) {
      case TypeKind::Struct: {
        analyseScope(analyser, module,
                     child->_struct.inst->_struct.definitions);
        break;
      }
      case TypeKind::Enum: {
        analyseScope(analyser, module, child->_enum.inst->_enum.definitions);
        break;
      }
      case TypeKind::Union: {
        analyseScope(analyser, module, child->_union.inst->_union.definitions);
        break;
      }
      case TypeKind::Namespace: {
        analyseScope(analyser, module,
                     child->_namespace.inst->_namespace.definitions);
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
    if (inst->function.globals != nullptr) {
      analyseScope(analyser, module, inst->function.globals);
      for (size_t i = 0; i < inst->function.blocks.length; i++) {
        analyseBlock(analyser, module, inst->function.blocks.getUnchecked(i));
      }
    }
    break;
  }
  }
}
