#include "../uir.hpp"
#include "define.hpp"
#include "rir/constant.hpp"
#include "rir/rir.hpp"
#include "rir/type.hpp"
#include "uir/analysis/analysis.hpp"
#include "uir/literal.hpp"

void analyseGlobal(UIRAnalyser *analyser, UIRModule *module, UIRValue *inst) {
  switch (inst->kind) {
  case UIRValueKind::GlobalVariable: {
    // Type
    Option<RIRTypeId> typeId = {};
    if (inst->global_variable.type.isSome()) {
      // Get Type
      UIRValue *type_inst = inst->global_variable.type.get();
      UIRLiteral type_literal =
          analyser->comptime_state.execute(module, type_inst);
      typeId.setSome(type_literal.data._typeid);

      expect(type_literal.data.kind == UIRRawDataKind::TypeId,
             type_inst->source_location, "Field type must be a typeid");
    }

    // Analyse Constant
    if (inst->global_variable.constant.isSome()) {
      // Get Initial
      UIRValue *const_inst = inst->global_variable.constant.get();
      UIRLiteral const_literal =
          analyser->comptime_state.execute(module, const_inst);

      if (const_literal.data.kind == UIRRawDataKind::TypeId) {
        // FIXME: Pointer literal
        analyser->resolved_mapping.insert(
            inst, {.kind = UIRResolvedKind::Literal, .literal = const_literal});
      } else {
        RIRType *type =
            analyser->rir_ctx->getType(const_literal.lit_type.get());
        expect(type != nullptr, const_inst->source_location,
               "Couldn't determine type of constant");

        if (typeId.isNone()) {
          typeId.setSome(const_literal.lit_type.get());
        } else {
          RIRType *expected_type = analyser->rir_ctx->getType(typeId.get());
          // TODO: Compile-time Cast
          // autoCast(analyser, const_inst, inst->result_type->child);
          expect(expected_type->compare(analyser->rir_ctx->types, type->id),
                 const_inst->source_location,
                 "Field initial doesn't match type. Field Type: `"
                     << expected_type->id << "` Initial Type: `" << type->id
                     << "`\n");
        }

        // Create Instruction
        RIRConstant constant = uirRawDataToRIRConstant(const_literal.data);
        RIRValueId out_id = analyser->builder.buildGlobalVariable(
            typeId.get(), constant, inst->name);
        analyser->resolved_mapping.insert(
            inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
      }
    } else {
      RIRValueId out_id =
          analyser->builder.buildGlobalVariable(typeId.get(), {}, inst->name);
      analyser->resolved_mapping.insert(
          inst, {.kind = UIRResolvedKind::Inst, .inst = out_id});
    }

    // Analyse Definitions
    UIRResolved *resolved = analyser->resolved_mapping.get(inst);
    if (resolved->kind == UIRResolvedKind::Literal) {
      if (resolved->literal.data.ptr.data->kind == UIRRawDataKind::TypeId) {
        RIRType *ptr_type =
            analyser->rir_ctx->getType(resolved->literal.lit_type.get());
        RIRType *child_type = analyser->rir_ctx->getType(ptr_type->child);
        switch (child_type->kind) {
        case RIRTypeKind::Struct: {
          UIRValue *_struct =
              reinterpret_cast<UIRValue *>(child_type->_struct.unique);
          analyseScope(analyser, module, _struct->_struct.definitions);
          break;
        }
        case RIRTypeKind::Enum: {
          UIRValue *_enum =
              reinterpret_cast<UIRValue *>(child_type->_enum.unique);
          analyseScope(analyser, module, _enum->_enum.definitions);
          break;
        }
        case RIRTypeKind::Union: {
          UIRValue *_union =
              reinterpret_cast<UIRValue *>(child_type->_union.unique);
          analyseScope(analyser, module, _union->_union.definitions);
          break;
        }
        }
      } else if (resolved->literal.data.ptr.data->kind ==
                 UIRRawDataKind::Namespace) {
        analyseScope(analyser, module,
                     resolved->literal.data.ptr.data->_namespace->_namespace
                         .definitions);
      }
    }
    break;
  }
  case UIRValueKind::Function: {
    UIRLiteral type = analyser->comptime_state.execute(module, inst);

    RIRValueId fn_inst_id = analyser->builder.buildFunction(
        type.data._typeid, inst->function.undefined, inst->name);
    analyser->resolved_mapping.insert(
        inst, {.kind = UIRResolvedKind::Inst, .inst = fn_inst_id});

    // Analyse Body
    if (inst->function.globals != nullptr) {
      analyseScope(analyser, module, inst->function.globals);

      for (size_t i = 0; i < inst->function.blocks.length; i++) {
        UIRBlock *uir_block = inst->function.blocks.getUnchecked(i);
        RIRBlockId rir_block = analyser->builder.appendBlock(fn_inst_id);
        analyser->resolved_block_mapping.insert(uir_block, rir_block);
        analyser->builder.block.setSome(analyser->rir_ctx->getBlock(rir_block));
      }

      for (size_t i = 0; i < inst->function.blocks.length; i++) {
        UIRBlock *uir_block = inst->function.blocks.getUnchecked(i);
        RIRBlockId rir_block_id =
            *analyser->resolved_block_mapping.get(uir_block);
        analyser->builder.block.setSome(
            analyser->rir_ctx->getBlock(rir_block_id));

        analyseBlock(analyser, module, uir_block);
      }
    }
    break;
  }
  }
}
