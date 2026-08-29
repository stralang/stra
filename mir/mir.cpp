#include "mir.hpp"
#include "allocator.hpp"
#include <cstdlib>
#include <iostream>

void MIRContext::init(Allocator *allocator) {
  this->allocator = allocator;
  this->arena.init(allocator, 1024 * 1024 * 8);
}

void MIRContext::deinit() { this->arena.deinit(); }

void MIRModule::init(Allocator *allocator) {
  this->arena.init(allocator, 1024 * 1024 * 8);
  this->definitions = (MIRScope *)this->arena.alloc(sizeof(MIRScope));
  this->definitions->list.init(allocator, 32);
  this->definitions->owner = nullptr;
}

void MIRModule::deinit() {
  this->arena.deinit();
  this->definitions->list.deinit();
}

MIRValue *MIRContext::make(MIRValue value) {
  MIRValue *ptr = (MIRValue *)this->arena.alloc(sizeof(MIRValue));
  *ptr = value;
  return ptr;
}

MIRValue *MIRContext::makeLiteral(MIRLiteral lit) {
  MIRValue inst = {.kind = MIRValueKind::Literal};
  inst.literal = lit;
  return this->make(inst);
}

bool MIRBlock::hasTerminator() {
  size_t i = this->instructions.length;
  while (i > 0) {
    i -= 1;
    MIRValue *inst = this->instructions.getUnchecked(i);
    if (inst->kind == MIRValueKind::Return) {
      return true;
    }
  }

  return false;
}

MIRBlock *MIRBuilder::appendBlock(MIRValue *parent, String name) {
  MIRBlock *block = (MIRBlock *)this->module->arena.alloc(sizeof(MIRBlock));
  block->id = this->module->next_id;
  block->name = name;
  block->parent = parent;
  this->module->next_id += 1;

  block->instructions.init(this->module->arena.allocator, 32);
  if (parent->kind == MIRValueKind::Function) {
    parent->function.blocks.push(block);
  } else if (parent->kind == MIRValueKind::Comptime) {
    parent->comptime.blocks.push(block);
  }

  return block;
}

MIRValue *MIRBuilder::insert(MIRValue inst, bool global, String name) {
  inst.id = this->module->next_id;
  inst.name = name;
  this->module->next_id += 1;

  MIRValue *ptr_inst = (MIRValue *)this->module->arena.alloc(sizeof(MIRValue));
  *ptr_inst = inst;
  if (this->block != nullptr) {
    ptr_inst->parent = this->block;
    this->block->instructions.push(ptr_inst);
  } else if (this->scope != nullptr) {
    this->scope->list.push(ptr_inst);
  } else {
    std::cerr << "Block or Scope must be provided to insert instruction.\n";
    std::abort();
  }
  return ptr_inst;
}

MIRValue *MIRBuilder::buildAlloca(MIRValue *type, String name) {
  MIRValue inst = {.kind = MIRValueKind::Alloca};
  inst.alloca = {type};
  return this->insert(inst, false, name);
}

MIRValue *MIRBuilder::buildLoad(MIRValue *ptr, String name) {
  MIRValue inst = {.kind = MIRValueKind::Load};
  inst.load.ptr = ptr;
  return this->insert(inst, false, name);
}

MIRValue *MIRBuilder::buildStore(MIRValue *value, MIRValue *ptr) {
  MIRValue inst = {.kind = MIRValueKind::Store};
  inst.store = {.value = value, .ptr = ptr};
  return this->insert(inst);
}

MIRValue *MIRBuilder::buildArg(MIRValue *type, String name) {
  MIRValue inst = {.kind = MIRValueKind::Arg};
  inst.arg.type = type;
  return this->insert(inst, false, name);
}

MIRValue *MIRBuilder::buildBinOp(MIRValue *lhs, MIRValue *rhs, MIROpcode opcode,
                                 String name) {
  MIRValue inst = {.kind = MIRValueKind::BinOp};
  inst.binop = {.opcode = opcode, .lhs = lhs, .rhs = rhs};
  return this->insert(inst, false, name);
}

MIRValue *MIRBuilder::buildUnaryOp(MIRValue *value, MIROpcode opcode,
                                   String name) {
  MIRValue inst = {.kind = MIRValueKind::UnaryOp};
  inst.unaryop = {.opcode = opcode, .value = value};
  return this->insert(inst, false, name);
}

MIRValue *MIRBuilder::buildCall(MIRValue *callee, Slice<MIRValue *> arguments,
                                MIRValue *receiver, String name) {
  MIRValue inst = {.kind = MIRValueKind::Call};
  inst.call = {.callee = callee, .arguments = arguments, .receiver = receiver};
  return this->insert(inst, false, name);
}

MIRValue *MIRBuilder::buildGEP(MIRValue *ptr, MIRValue *index, String name) {
  MIRValue inst = {.kind = MIRValueKind::GEP};
  inst.gep = {.ptr = ptr, .index = index};
  return this->insert(inst, false, name);
}

MIRValue *MIRBuilder::buildMemberAccess(MIRValue *parent, String member,
                                        String name) {
  MIRValue inst = {.kind = MIRValueKind::MemberAccess};
  inst.member_access.parent = parent;
  inst.member_access.member = member;
  return this->insert(inst, false, name);
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

MIRValue *MIRBuilder::buildSwitch(MIRValue *value, MIRBlock *default_block,
                                  size_t cases) {
  MIRValue inst = {.kind = MIRValueKind::Switch};
  inst._switch.condition = value;
  inst._switch.default_block = default_block;

  uint8_t *onval_ptr = this->module->arena.alloc(sizeof(void *) * cases);
  uint8_t *blocks_ptr = this->module->arena.alloc(sizeof(void *) * cases);
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

MIRValue *MIRBuilder::buildComptime(String name) {
  MIRValue inst = {.kind = MIRValueKind::Comptime};
  return this->insert(inst, false, name);
}
MIRValue *MIRBuilder::buildTypeOf(MIRValue *value, String name) {
  MIRValue inst = {.kind = MIRValueKind::TypeOf};
  inst._typeof = value;
  return this->insert(inst, true, name);
}

MIRValue *MIRBuilder::buildGlobalVariable(MIRValue *type, MIRValue *constant,
                                          String name) {
  MIRValue inst = {.kind = MIRValueKind::GlobalVariable};
  inst.global_variable = {type, constant};
  return this->insert(inst, true, name);
}

MIRValue *MIRBuilder::buildFunction(Slice<MIRValue *> parameters,
                                    MIRValue *return_type, String name) {
  MIRValue inst = {.kind = MIRValueKind::Function};
  inst.function = {
      .parameter_types = parameters,
      .return_type = return_type,
  };
  return this->insert(inst, true, name);
}
