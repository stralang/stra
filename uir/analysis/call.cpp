#include "define.hpp"
#include "rir/constant.hpp"
#include "uir/analysis/analysis.hpp"
#include "uir/uir.hpp"

void analyseComptimeCall(UIRAnalyser *analyser, UIRValue *inst,
                         UIRValue *callee_inst) {
  if (callee_inst->function.attributes.implied_comptime) {
    UIRLiteral lit = analyser->comptime_state.execute(inst);
    analyser->resolved_mapping.insert(
        inst->id, {.kind = UIRResolvedKind::Literal, .literal = lit});
    return;
  }

  expect(false, inst->source_location,
         "Cannot runtime analyse call to compile-time only function");
}

void analyseCall(UIRAnalyser *analyser, UIRValue *inst) {
  UIRValue *callee_inst = analyser->ctx->getInst(inst->call.callee);
  if (callee_inst->kind == UIRValueKind::Function &&
      callee_inst->function.comptime) {
    analyseComptimeCall(analyser, inst, callee_inst);
    return;
  }

  analyse(analyser, callee_inst);

  // Get Callee
  UIRResolved *callee_resolved =
      analyser->resolved_mapping.get(inst->call.callee);

  if (callee_resolved->kind == UIRResolvedKind::Literal) {
    // Handle function literal
    callee_inst =
        analyser->ctx->getInst(callee_resolved->literal.data.function);
    if (callee_inst->kind == UIRValueKind::Function &&
        callee_inst->function.comptime) {
      analyseComptimeCall(analyser, inst, callee_inst);
      return;
    }

    analyse(analyser, callee_inst);
    callee_resolved =
        analyser->resolved_mapping.get(callee_resolved->literal.data.function);
  }

  assert(callee_resolved->kind == UIRResolvedKind::Inst);
  RIRValue *resolved_callee_inst =
      analyser->rir_ctx->getInst(callee_resolved->inst);

  // Auto dereference
  RIRType *resolved_callee_type =
      analyser->rir_ctx->getType(resolved_callee_inst->result);
  if (resolved_callee_type->kind == RIRTypeKind::Pointer) {
    resolved_callee_type =
        analyser->rir_ctx->getType(resolved_callee_type->child);
  }

  // Get function
  expect(resolved_callee_type->kind == RIRTypeKind::Function,
         callee_inst->source_location,
         "Callee must be a function. Got " << resolved_callee_type << "`");

  RIRType *fn_type = resolved_callee_type;

  // Arguments
  Slice<RIRValueId> arguments = Slice<RIRValueId>{
      .ptr = (RIRValueId *)analyser->allocator->alloc(
          sizeof(RIRValueId) * resolved_callee_type->function.arguments.len),
      .len = resolved_callee_type->function.arguments.len,
  };

  // Get receiver
  size_t initial_idx = 0;
  if (inst->call.receiver.isSome()) {
    UIRValue *uir_receiver_inst =
        analyser->ctx->getInst(inst->call.receiver.get());
    analyse(analyser, uir_receiver_inst);

    UIRResolved *receiver_resolved =
        analyser->resolved_mapping.get(uir_receiver_inst->id);

    // The receiver is only valid if it is not a TypeId or Namespace
    bool receiver_is_valid =
        receiver_resolved->kind != UIRResolvedKind::Literal ||
        (receiver_resolved->literal.data.kind != UIRRawDataKind::TypeId &&
         receiver_resolved->literal.data.kind != UIRRawDataKind::Namespace);

    if (receiver_is_valid) {
      expect(fn_type->function.arguments.len >= 1,
             uir_receiver_inst->source_location,
             "Receiver expects method with atleast 1 argument");

      RIRValueId receiver_inst_id = getInstFromResolved(
          analyser, receiver_resolved, fn_type->function.arguments.ptr[0]);
      RIRValue *receiver_inst = analyser->rir_ctx->getInst(receiver_inst_id);
      RIRType *expected_type =
          analyser->rir_ctx->getType(fn_type->function.arguments.ptr[0]);
      expect(expected_type->compare(analyser->rir_ctx->types,
                                    receiver_inst->result),
             uir_receiver_inst->source_location,
             "Receiver `" << receiver_inst->result << "` doesn't match `"
                          << expected_type->id << "`");

      arguments[initial_idx] = receiver_inst->id;
      initial_idx = 1;
    }
  }

  // Analyse arguments
  for (size_t i = 0; i < inst->call.arguments.len; i++) {
    UIRValue *uir_arg = analyser->ctx->getInst(inst->call.arguments.ptr[i]);
    if (i > fn_type->function.arguments.len - initial_idx) {
      expect(false, uir_arg->source_location, "Too many arguments");
      break;
    }

    RIRTypeId expected_type_id =
        fn_type->function.arguments.ptr[i + initial_idx];
    RIRType *expected_type = analyser->rir_ctx->getType(expected_type_id);

    // Get argument
    analyse(analyser, uir_arg);
    UIRResolved *arg_resolved =
        analyser->resolved_mapping.get(uir_arg->id); // FIXME: Confirm kind
    RIRValueId resolved_arg_id =
        getInstFromResolved(analyser, arg_resolved, expected_type_id);

    // Cast and Compare
    RIRValue *resolved_arg_inst = analyser->rir_ctx->getInst(resolved_arg_id);
    resolved_arg_inst = autoCast(analyser, resolved_arg_inst, expected_type_id);
    expect(expected_type->compare(analyser->rir_ctx->types,
                                  resolved_arg_inst->result),
           uir_arg->source_location,
           "Argument `" << resolved_arg_inst->result
                        << "` doesn't match expected `" << expected_type
                        << "`");

    arguments[i + initial_idx] = resolved_arg_inst->id;
  }

  // Create Instruction
  RIRValueId out_id = analyser->builder.buildCall(
      resolved_callee_inst->id, arguments, fn_type->function._return);
  analyser->resolved_mapping.insert(
      inst->id, {.kind = UIRResolvedKind::Inst, .inst = out_id});
}
