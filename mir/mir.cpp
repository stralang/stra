#include "mir.hpp"
#include "allocator.hpp"

void MIRContext::init(Allocator *allocator) {
  this->allocator = allocator;
  this->values.init(allocator, 32);
}

void MIRContext::deinit() { this->values.deinit(); }

void MIRModule::init(Allocator *allocator) {
  this->allocator = allocator;
  this->instructions.init(this->allocator, 32);
}

void MIRModule::deinit() { this->instructions.deinit(); }

MIRValue *MIRContext::make(MIRValue value) {
  this->values.push(value);
  return this->values.back();
}

MIRValue *MIRContext::makeLiteral(MIRLiteral lit) {
  MIRValue inst = {.kind = MIRValueKind::Literal};
  inst.literal = lit;
  return this->make(inst);
}

MIRValue *MIRBuilder::insert(MIRValue inst, bool global) {
  if (this->block != nullptr && !global) {
    this->block->instructions.push(inst);
    return this->block->instructions.back();
  }

  this->module->instructions.push(inst);
  return this->module->instructions.back();
}

MIRValue *MIRBuilder::buildField(MIRValue *type, MIRValue *initial) {
  if (type == nullptr && initial == nullptr) {
    std::cerr << "Field must have type or initial provided.";
    return nullptr;
  }

  MIRValue inst = {.kind = MIRValueKind::Field};
  inst.field = {type, initial};
  return this->insert(inst);
}

MIRValue *MIRBuilder::buildLoad(MIRValue *ptr) {
  MIRValue inst = {.kind = MIRValueKind::Load};
  inst.load.ptr = ptr;
  return this->insert(inst);
}

MIRValue *MIRBuilder::buildStore(MIRValue *value, MIRValue *ptr) {
  MIRValue inst = {.kind = MIRValueKind::Store};
  inst.store = {.value = value, .ptr = ptr};
  return this->insert(inst);
}

MIRValue *MIRBuilder::buildArg(MIRValue *type) {
  MIRValue inst = {.kind = MIRValueKind::Arg};
  inst.arg.type = type;
  return this->insert(inst);
}

MIRValue *MIRBuilder::buildBinOp(MIRValue *lhs, MIRValue *rhs,
                                 MIROpcode opcode) {
  MIRValue inst = {.kind = MIRValueKind::BinOp};
  inst.binop = {.opcode = opcode, .lhs = lhs, .rhs = rhs};
  return this->insert(inst);
}

MIRValue *MIRBuilder::buildUnaryOp(MIRValue *value, MIROpcode opcode) {
  MIRValue inst = {.kind = MIRValueKind::UnaryOp};
  inst.unaryop = {.opcode = opcode, .value = value};
  return this->insert(inst);
}

MIRValue *MIRBuilder::buildCall(MIRValue *callee, Slice<MIRValue *> arguments) {
  MIRValue inst = {.kind = MIRValueKind::Call};
  inst.call = {.callee = callee, .arguments = arguments};
  return this->insert(inst);
}

MIRValue *MIRBuilder::buildGEP(MIRValue *ptr, MIRValue *index) {
  MIRValue inst = {.kind = MIRValueKind::GEP};
  inst.gep = {.ptr = ptr, .index = index};
  return this->insert(inst);
}

// If `value` is null then this returns `void`
MIRValue *MIRBuilder::buildReturn(MIRValue *value) {
  MIRValue inst = {.kind = MIRValueKind::Return};
  inst.ret = {.value = value};
  return this->insert(inst);
}

MIRValue *MIRBuilder::buildBr(MIRBlock *block) {
  MIRValue inst = {.kind = MIRValueKind::Branch};
  inst.br = block;
  return this->insert(inst);
}

MIRValue *MIRBuilder::buildCondBr(MIRValue *condition, MIRBlock *then,
                                  MIRBlock *_else) {
  MIRValue inst = {.kind = MIRValueKind::CondBranch};
  inst.condbr = {.condition = condition, .then = then, ._else = _else};
  return this->insert(inst);
}

MIRValue *MIRBuilder::buildSwitch(MIRValue *value, size_t cases) {
  MIRValue inst = {.kind = MIRValueKind::Switch};
  inst._switch.condition = value;

  uint8_t *onval_ptr = this->module->allocator->alloc(sizeof(void *) * cases);
  uint8_t *blocks_ptr = this->module->allocator->alloc(sizeof(void *) * cases);
  inst._switch.onvals = {.ptr = (MIRValue **)onval_ptr, .len = cases};
  inst._switch.blocks = {.ptr = (MIRBlock **)blocks_ptr, .len = cases};
  inst._switch.slots = 0;

  return this->insert(inst);
}

void MIRBuilder::addCase(MIRValue *switch_inst, MIRValue *onval,
                         MIRBlock *then) {
  assert(switch_inst->kind == MIRValueKind::Switch &&
         "Cannot add switch case to non-switch instruction");
  assert(switch_inst->_switch.slots < switch_inst->_switch.onvals.len &&
         "Switch instruction is already full");

  switch_inst->_switch.onvals[switch_inst->_switch.slots] = onval;
  switch_inst->_switch.blocks[switch_inst->_switch.slots] = then;
  switch_inst->_switch.slots += 1;
}
