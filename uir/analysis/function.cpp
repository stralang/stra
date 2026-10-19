#include "define.hpp"
#include "uir/uir.hpp"

void analyseFunction(UIRAnalyser *analyser, UIRValue *inst) {
  assert(inst->kind == UIRValueKind::Function);
  String link_name = getLinkName(analyser, inst);

  expect(inst->function.attributes.builtin.isAbsent() ||
             inst->function.undefined,
         inst->source_location, "Builtin function must be undefined");

  if (inst->function.comptime) {
    return;
  }

  // Save builder state
  Option<RIRBlock *> prev_block = analyser->builder.block;

  // Build and Map Function
  UIRLiteral type = analyser->comptime_state.execute(inst);
  RIRValueId fn_inst_id = analyser->builder.buildFunction(
      type.data._typeid, inst->function.undefined, link_name);
  analyser->resolved_mapping.insert(
      inst->id, {.kind = UIRResolvedKind::Inst, .inst = fn_inst_id});

  // Analyse Body
  if (inst->function.undefined) {
    // TODO: Execute body to compute type
  } else if (inst->function.blocks.length > 0) {
    for (size_t i = 0; i < inst->function.blocks.length; i++) {
      UIRBlockId uir_block = inst->function.blocks.getUnchecked(i);
      RIRBlockId rir_block = analyser->builder.appendBlock(fn_inst_id);
      analyser->resolved_block_mapping.insert(uir_block, rir_block);
      analyser->builder.block.setSome(analyser->rir_ctx->getBlock(rir_block));
    }

    for (size_t i = 0; i < inst->function.blocks.length; i++) {
      UIRBlockId uir_block = inst->function.blocks.getUnchecked(i);
      RIRBlockId rir_block_id =
          *analyser->resolved_block_mapping.get(uir_block);
      analyser->builder.block.setSome(
          analyser->rir_ctx->getBlock(rir_block_id));

      analyseBlock(analyser, analyser->ctx->getBlock(uir_block));
    }
  }

  // Load builder state
  analyser->builder.block = prev_block;
}
