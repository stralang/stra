#include "mir.hpp"
#include "allocator.hpp"
#include <cstdlib>
#include <iostream>

void MIRContext::init(Allocator *allocator) {
  this->allocator = allocator;
  this->arena.init(allocator, 1024 * 1024 * 8);
  this->modules.init(allocator, 32);
}

void MIRContext::deinit() { this->arena.deinit(); }

void MIRModule::init(Allocator *allocator) {
  this->arena.init(allocator, 1024 * 1024 * 8);
  this->instrs.init(allocator, 256);
  this->blocks.init(allocator, 64);
  this->scopes.init(allocator, 32);

  MIRScope root = {
      .id = {.module = this->id, .local = 0},
      .owner = {.module = this->id, .local = 0xFFFFFFFF},
  };
  root.list.init(allocator, 32);
  this->definitions = root.id;
  this->scopes.push(root);
}

void MIRModule::deinit() {
  this->arena.deinit();
  this->instrs.deinit();
  this->blocks.deinit();
  this->scopes.deinit();
}

MIRValueId MIRContext::make(MIRValue value) {
  // MIRValueId ptr = (MIRValueId)this->arena.alloc(sizeof(MIRValue));
  // *ptr = value;
  // return ptr;
  std::cerr << __FILE__ << ":" << __LINE__ << " FIXME\n";
  std::abort();
  // FIXME:
}

MIRValueId MIRContext::makeLiteral(MIRLiteral lit) {
  MIRValue inst = {.kind = MIRValueKind::Literal};
  inst.literal = lit;
  return this->make(inst);
}

bool MIRBlock::hasTerminator(MIRModule *module) {
  size_t i = this->instructions.length;
  while (i > 0) {
    i -= 1;
    MIRValueId id = this->instructions.getUnchecked(i);
    MIRValue *inst = module->getInstr(id);
    if (inst->kind == MIRValueKind::Return) {
      return true;
    }
  }

  return false;
}

void MIRBuilder::setSourceLocation(MIRValueId inst, SrcLoc location) {
  MIRValue *value = this->module->getInstr(inst);
  value->source_location = location;
}

MIRBlockId MIRBuilder::appendBlock(MIRValueId parent, String name) {
  MIRBlock block{
      .name = name,
      .parent = parent,
  };
  block.id = {
      .module = this->module->id,
      .local = (uint32_t)this->module->blocks.length,
  };

  block.instructions.init(this->module->arena.allocator, 32);
  MIRValue *parent_inst = this->module->getInstr(parent);
  if (parent_inst->kind == MIRValueKind::Function) {
    parent_inst->function.blocks.push(block.id);
  } else if (parent_inst->kind == MIRValueKind::Comptime) {
    parent_inst->comptime.blocks.push(block.id);
  }

  this->module->blocks.push(block);
  return block.id;
}

MIRScopeId MIRBuilder::makeScope(MIRValueId parent) {
  MIRScope scope = {
      .owner = parent,
  };
  scope.id = {
      .module = this->module->id,
      .local = (uint32_t)this->module->scopes.length,
  };

  scope.list.init(this->module->arena.allocator, 32);
  this->module->scopes.push(scope);

  return scope.id;
}

MIRValueId MIRBuilder::insert(MIRValue inst, bool global, String name) {
  inst.id = {
      .module = this->module->id,
      .local = (uint32_t)this->module->instrs.length,
  };
  inst.name = name;

  if (this->block.isSome()) {
    inst.parent = this->block.get();
    this->module->getBlock(inst.parent)->instructions.push(inst.id);
  } else if (this->scope.isSome()) {
    this->module->getScope(this->scope.get())->list.push(inst.id);
  } else {
    std::cerr << "Block or Scope must be provided to insert instruction.\n";
    std::abort();
  }

  this->module->instrs.push(inst);
  return inst.id;
}

MIRValueId MIRBuilder::buildAlloca(MIRValueId type, String name) {
  MIRValue inst = {.kind = MIRValueKind::Alloca};
  inst.alloca = {type};
  return this->insert(inst, false, name);
}

MIRValueId MIRBuilder::buildLoad(MIRValueId ptr, String name) {
  MIRValue inst = {.kind = MIRValueKind::Load};
  inst.load.ptr = ptr;
  return this->insert(inst, false, name);
}

MIRValueId MIRBuilder::buildStore(MIRValueId value, MIRValueId ptr) {
  MIRValue inst = {.kind = MIRValueKind::Store};
  inst.store = {.value = value, .ptr = ptr};
  return this->insert(inst);
}

MIRValueId MIRBuilder::buildArg(MIRValueId type, String name) {
  MIRValue inst = {.kind = MIRValueKind::Arg};
  inst.arg.type = type;
  return this->insert(inst, false, name);
}

MIRValueId MIRBuilder::buildBinOp(MIRValueId lhs, MIRValueId rhs,
                                  MIROpcode opcode, String name) {
  MIRValue inst = {.kind = MIRValueKind::BinOp};
  inst.binop = {.opcode = opcode, .lhs = lhs, .rhs = rhs};
  return this->insert(inst, false, name);
}

MIRValueId MIRBuilder::buildUnaryOp(MIRValueId value, MIROpcode opcode,
                                    String name) {
  MIRValue inst = {.kind = MIRValueKind::UnaryOp};
  inst.unaryop = {.opcode = opcode, .value = value};
  return this->insert(inst, false, name);
}

MIRValueId MIRBuilder::buildCall(MIRValueId callee, Slice<MIRValueId> arguments,
                                 Option<MIRValueId> receiver, String name) {
  MIRValue inst = {.kind = MIRValueKind::Call};
  inst.call = {.callee = callee, .arguments = arguments, .receiver = receiver};
  return this->insert(inst, false, name);
}

MIRValueId MIRBuilder::buildGEP(MIRValueId ptr, MIRValueId index, String name) {
  MIRValue inst = {.kind = MIRValueKind::GEP};
  inst.gep = {.ptr = ptr, .index = index};
  return this->insert(inst, false, name);
}

MIRValueId MIRBuilder::buildLookup(MIRValueId parent, String member,
                                   String name) {
  MIRValue inst = {.kind = MIRValueKind::Lookup};
  inst.lookup.parent = parent;
  inst.lookup.member = member;
  return this->insert(inst, false, name);
}

// If `value` is null then this returns `void`
MIRValueId MIRBuilder::buildReturn(Option<MIRValueId> value) {
  MIRValue inst = {.kind = MIRValueKind::Return};
  inst.ret = {.value = value};
  return this->insert(inst);
}

MIRValueId MIRBuilder::buildBr(MIRBlockId block) {
  MIRValue inst = {.kind = MIRValueKind::Branch};
  inst.br = block;
  return this->insert(inst);
}

MIRValueId MIRBuilder::buildCondBr(MIRValueId condition, MIRBlockId then,
                                   MIRBlockId _else) {
  MIRValue inst = {.kind = MIRValueKind::CondBranch};
  inst.condbr = {.condition = condition, .then = then, ._else = _else};
  return this->insert(inst);
}

MIRValueId MIRBuilder::buildSwitch(MIRValueId value, MIRBlockId default_block,
                                   size_t cases) {
  MIRValue inst = {.kind = MIRValueKind::Switch};
  inst._switch.condition = value;
  inst._switch.default_block = default_block;

  uint8_t *onval_ptr = this->module->arena.alloc(sizeof(void *) * cases);
  uint8_t *blocks_ptr = this->module->arena.alloc(sizeof(void *) * cases);
  inst._switch.onvals = {.ptr = (MIRValueId *)onval_ptr, .len = cases};
  inst._switch.blocks = {.ptr = (MIRBlockId *)blocks_ptr, .len = cases};
  inst._switch.slots = 0;

  return this->insert(inst);
}

void MIRBuilder::addCase(MIRValueId _switch, MIRValueId onval,
                         MIRBlockId then) {
  MIRValue *switch_inst = this->module->getInstr(_switch);
  assert(switch_inst->kind == MIRValueKind::Switch &&
         "Cannot add switch case to non-switch instruction");
  assert(switch_inst->_switch.slots < switch_inst->_switch.onvals.len &&
         "Switch instruction is already full");

  switch_inst->_switch.onvals[switch_inst->_switch.slots] = onval;
  switch_inst->_switch.blocks[switch_inst->_switch.slots] = then;
  switch_inst->_switch.slots += 1;
}

MIRValueId MIRBuilder::buildComptime(String name) {
  MIRValue inst = {.kind = MIRValueKind::Comptime};
  inst.comptime.blocks.init(this->module->arena.allocator, 32);
  return this->insert(inst, false, name);
}
MIRValueId MIRBuilder::buildTypeOf(MIRValueId value, String name) {
  MIRValue inst = {.kind = MIRValueKind::TypeOf};
  inst._typeof = value;
  return this->insert(inst, true, name);
}

MIRValueId MIRBuilder::buildGlobalVariable(Option<MIRValueId> type,
                                           Option<MIRValueId> constant,
                                           String name) {
  MIRValue inst = {.kind = MIRValueKind::GlobalVariable};
  inst.global_variable = {type, constant};
  return this->insert(inst, true, name);
}

MIRValueId MIRBuilder::buildFunction(Slice<MIRValueId> parameters,
                                     Option<MIRValueId> return_type,
                                     String name) {
  MIRValue inst = {.kind = MIRValueKind::Function};
  inst.function = {
      .parameter_types = parameters,
      .return_type = return_type,
  };
  return this->insert(inst, true, name);
}

MIRValueId MIRBuilder::buildStruct(Slice<MIRStruct::Field> fields,
                                   String name) {
  MIRValue inst = {.kind = MIRValueKind::Struct};
  inst._struct.fields = fields;
  return this->insert(inst, false, name);
}

MIRValueId MIRBuilder::buildEnum(MIRValueId repr_type,
                                 Slice<MIREnum::Member> members, String name) {
  MIRValue inst = {.kind = MIRValueKind::Enum};
  inst._enum.repr_type = repr_type;
  inst._enum.members = members;
  return this->insert(inst, false, name);
}

MIRValueId MIRBuilder::buildUnion(MIRValueId repr_type,
                                  Slice<MIRStruct::Field> variants,
                                  String name) {
  MIRValue inst = {.kind = MIRValueKind::Union};
  inst._union.repr_type = repr_type;
  inst._union.variants = variants;
  return this->insert(inst, false, name);
}

MIRValueId MIRBuilder::buildNamespace(String name) {
  MIRValue inst = {.kind = MIRValueKind::Namespace};
  return this->insert(inst, false, name);
}

MIRValueId MIRBuilder::buildSlice(MIRValueId element, Option<MIRValueId> length,
                                  bool is_pointer, String name) {
  MIRValue inst = {.kind = MIRValueKind::Slice};
  inst.slice = {.element = element, .length = length, .is_pointer = is_pointer};
  return this->insert(inst, false, name);
}

MIRValue *MIRContext::getInstr(MIRValueId id) {
  return this->modules.getUnchecked(id.module)->getInstr(id);
}
MIRBlock *MIRContext::getBlock(MIRBlockId id) {
  return this->modules.getUnchecked(id.module)->getBlock(id);
}
MIRScope *MIRContext::getScope(MIRScopeId id) {
  return this->modules.getUnchecked(id.module)->getScope(id);
}

MIRValue *MIRModule::getInstr(MIRValueId id) {
  return this->instrs.getPtrUnchecked(id.local);
}
MIRBlock *MIRModule::getBlock(MIRBlockId id) {
  return this->blocks.getPtrUnchecked(id.local);
}
MIRScope *MIRModule::getScope(MIRScopeId id) {
  return this->scopes.getPtrUnchecked(id.local);
}
