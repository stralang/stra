#include "define.hpp"
#include "uir/analysis/analysis.hpp"
#include "uir/uir.hpp"

void computeUndefinedFnType(UIRAnalyser *analyser, UIRValue *fn_inst) {
  UIRBlockId block_id = fn_inst->function.blocks.get(0);
  UIRBlock *block = analyser->ctx->getBlock(block_id);

  UIRFunctionInfo *info = analyser->function_stack.back();
  RIRTypeId return_type;

  analyser->comptime_state.pushStack();
  for (size_t i = 0; i < block->instructions.length; i++) {
    UIRValueId inst_id = block->instructions.get(i);
    UIRValue *inst = analyser->ctx->getInst(inst_id);

    if (inst->kind == UIRValueKind::Arg) {
      UIRValue *type_inst = analyser->ctx->getInst(inst->arg.type);
      UIRLiteral arg_literal = analyser->comptime_state.execute(type_inst);
      expect(arg_literal.data.kind == UIRRawDataKind::TypeId,
             type_inst->source_location, "Argument type must be a type");

      info->parameter_types.push(arg_literal.data._typeid);
    } else if (inst->kind == UIRValueKind::RetType) {
      UIRValue *type_inst = analyser->ctx->getInst(inst->arg.type);
      UIRLiteral type_literal = analyser->comptime_state.execute(type_inst);
      expect(type_literal.data.kind == UIRRawDataKind::TypeId,
             type_inst->source_location, "Return type must be a type");

      return_type = type_literal.data._typeid;
      break;
    }
  }
  analyser->comptime_state.popStack();

  RIRTypeId fn_type = analyser->rir_ctx->types->push({
      .kind = RIRTypeKind::Function,
      .function = {.arguments = info->parameter_types.slice(),
                   ._return = return_type},
  });

  RIRValue *rir_inst = analyser->rir_ctx->getInst(info->rir);
  rir_inst->result = fn_type;
  rir_inst->function.type = fn_type;

  analyser->resolved_mapping.get(fn_inst->id)->kind = UIRResolvedKind::Inst;
}

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
  RIRValueId fn_inst_id =
      analyser->builder.buildFunction({}, inst->function.undefined, link_name);
  analyser->resolved_mapping.insert(
      inst->id, {.kind = UIRResolvedKind::Pending, .inst = fn_inst_id});

  // Push stack
  UIRFunctionInfo stack_entry = {.creator = inst->id, .rir = fn_inst_id};
  stack_entry.parameter_types.init(analyser->allocator, 32);
  analyser->function_stack.push(stack_entry);

  // Analyse Body
  if (inst->function.undefined) {
    computeUndefinedFnType(analyser, inst);
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

  // Pop stack
  analyser->function_stack.pop();

  // Load builder state
  analyser->builder.block = prev_block;
}
