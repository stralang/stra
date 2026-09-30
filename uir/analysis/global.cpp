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
      analyser->resolved_mapping.insert(
          const_inst,
          {.kind = UIRResolvedKind::Literal, .literal = const_literal});

      if (const_literal.data.kind == UIRRawDataKind::TypeId) {
        // Generate Virtual Variable
        UIRResolved *const_resolved =
            analyser->resolved_mapping.get(const_inst);
        const_resolved->literal.lit_type =
            analyser->rir_ctx->types->push({.kind = RIRTypeKind::TypeId});

        // Create Pointer
        RIRTypeId ptr_type = analyser->rir_ctx->types->push({
            .kind = RIRTypeKind::Pointer,
            .child = const_resolved->literal.lit_type.get(),
        });
        UIRLiteral out_lit = {.data = {.kind = UIRRawDataKind::Pointer},
                              .lit_type = ptr_type};
        out_lit.data.ptr = {.kind = UIRPlaceKind::Raw,
                            .data = &const_resolved->literal.data};

        // Create mapping
        analyser->resolved_mapping.insert(
            inst, {.kind = UIRResolvedKind::Literal, .literal = out_lit});
      } else {
        // Generate Real Variable
        RIRType *type = nullptr;
        if (const_literal.lit_type.isSome()) {
          type = analyser->rir_ctx->getType(const_literal.lit_type.get());
        } else if (typeId.isSome()) {
          type = analyser->rir_ctx->getType(typeId.get());
          expect(
              compareRawDataToType(analyser, const_literal.data.kind, type->id),
              const_inst->source_location,
              "Raw data kind doesn't match type kind");
        } else {
          expect(false, inst->source_location,
                 "Global variable couldn't infer type from constant");
        }

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
