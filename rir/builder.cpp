#include "builder.hpp"
#include "rir.hpp"
#include <iostream>

RIRValueId RIRBuilder::build(RIRValue inst) {
  RIRValueId id = {this->module->id,
                   (uint32_t)this->module->instructions.len()};
  inst.id = id;
  this->module->instructions.push(inst);
  return id;
}

RIRValueId RIRBuilder::buildDeclare(RIRValue inst) {
  RIRValueId id = this->build(inst);
  this->module->roots.push(id);
  return id;
}

RIRValueId RIRBuilder::buildInst(RIRValue inst) {
  RIRValueId id = this->build(inst);
  this->block.get()->instructions.push(id);
  return id;
}

RIRValueId RIRBuilder::buildLocalVariable(RIRTypeId type, RIRTypeId result) {
  RIRValue inst = {.kind = RIRValueKind::LocalVariable, .result = result};
  inst.local = {.type = type};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildLoad(RIRValueId ptr, RIRTypeId result) {
  RIRValue inst = {.kind = RIRValueKind::Load, .result = result};
  inst.load = {.ptr = ptr};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildStore(RIRValueId ptr, RIRValueId value,
                                  RIRTypeId result) {
  RIRValue inst = {.kind = RIRValueKind::Store, .result = result};
  inst.store = {.ptr = ptr, .value = value};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildArg(RIRTypeId result) {
  RIRValue inst = {.kind = RIRValueKind::Arg, .result = result};
  inst.arg = {.type = result};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildBinOp(RIROpcode opcode, RIRValueId lhs,
                                  RIRValueId rhs, RIRTypeId result) {
  RIRValue inst = {.kind = RIRValueKind::BinOp, .result = result};
  inst.binop = {.opcode = opcode, .lhs = lhs, .rhs = rhs};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildUnaryOp(RIROpcode opcode, RIRValueId child,
                                    RIRTypeId result) {
  RIRValue inst = {.kind = RIRValueKind::UnaryOp, .result = result};
  inst.unaryop = {.opcode = opcode, .value = child};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildCall(RIRValueId callee, Slice<RIRValueId> arguments,
                                 RIRTypeId result) {
  RIRValue inst = {.kind = RIRValueKind::Call, .result = result};
  inst.call = {.callee = callee, .arguments = arguments};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildGEP(RIRValueId ptr, RIRValueId index,
                                RIRTypeId result) {
  RIRValue inst = {.kind = RIRValueKind::GEP, .result = result};
  inst.gep = {.ptr = ptr, .index = index};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildReturn(Option<RIRValueId> value) {
  RIRValue inst = {.kind = RIRValueKind::Return}; // TODO: void result
  inst.ret = {.value = value};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildBranch(RIRBlockId dest) {
  RIRValue inst = {.kind = RIRValueKind::Branch}; // TODO: void result
  inst.branch = dest;
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildCondBranch(RIRValueId condition, RIRBlockId then,
                                       RIRBlockId _else) {
  RIRValue inst = {.kind = RIRValueKind::CondBranch}; // TODO: void result
  inst.cond_branch = {.condition = condition, .then = then, ._else = _else};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildSwitch(RIRValueId condition,
                                   RIRBlockId default_dest, size_t case_count) {
  RIRValue inst = {.kind = RIRValueKind::Switch}; // TODO: void result
  inst._switch = {.condition = condition, .default_block = default_dest};
  inst._switch.onvals = {
      .ptr = (RIRValueId *)this->module->allocator->alloc(sizeof(RIRValueId) *
                                                          case_count),
      .len = case_count,
  };
  inst._switch.blocks = {
      .ptr = (RIRBlockId *)this->module->allocator->alloc(sizeof(RIRBlockId) *
                                                          case_count),
      .len = case_count,
  };
  return this->buildInst(inst);
}

void RIRBuilder::addCase(RIRValueId _switch, RIRValueId on_val, RIRBlockId dest,
                         size_t _case) {
  RIRValue *inst = this->ctx->getInst(_switch);
  assert(inst->kind == RIRValueKind::Switch);
  inst->_switch.onvals[_case] = on_val;
  inst->_switch.blocks.ptr[_case] = dest;
}

RIRValueId RIRBuilder::buildGlobalVariable(RIRTypeId type,
                                           Option<RIRValueId> constant,
                                           RIRTypeId result) {
  RIRValue inst = {.kind = RIRValueKind::Load, .result = result};
  inst.global_variable = {.type = type, .constant = constant};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildFunction(RIRTypeId type, bool undefined) {
  RIRValue inst = {.kind = RIRValueKind::Load, .result = type};
  inst.function = {.type = type, .undefined = undefined};
  if (!undefined) {
    inst.function.blocks.init(this->module->allocator, 32);
  }
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildConstant(RIRTypeId type, RIRConstant constant) {
  RIRValue inst = {.kind = RIRValueKind::Constant, .result = type};
  inst.constant = {.type = type, .value = constant};
  return this->build(inst);
}

void RIRBuilder::addDebugLocation(RIRValueId inst, SrcLoc location) {
  std::cerr << "TODO: Implement debug information. This is not a crash\n";
  // TODO: Implement debug information
}

void RIRBuilder::addDebugName(RIRValueId inst, String name) {
  std::cerr << "TODO: Implement debug information. This is not a crash\n";
  // TODO: Implement debug information
}
