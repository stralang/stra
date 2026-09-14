#include "builder.hpp"
#include "uir.hpp"

UIRBlock *UIRBuilder::appendBlock(UIRValue *parent, String name) {
  size_t idx = this->module->blocks.len();
  UIRBlock raw_block = {
      .id = idx,
      .name = name,
      .parent = parent,
  };
  raw_block.instructions.init(this->module->allocator, 32);

  this->module->blocks.push(raw_block);
  UIRBlock *block = this->module->blocks.getPtrUnchecked(idx);

  if (parent->kind == UIRValueKind::Function) {
    parent->function.blocks.push(block);
  } else if (parent->kind == UIRValueKind::Comptime) {
    parent->comptime.blocks.push(block);
  }
  return block;
}

UIRValue *UIRBuilder::insert(UIRValue inst, bool global, String name) {
  size_t idx = this->module->instructions.len();
  inst.id = idx;
  inst.name = name;

  this->module->instructions.push(inst);
  UIRValue *ptr_inst = this->module->instructions.getPtrUnchecked(idx);

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

UIRValue *UIRBuilder::buildLocalVariable(UIRValue *type, String name) {
  UIRValue inst = {.kind = UIRValueKind::LocalVariable};
  inst.local_variable = {type};
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildLoad(UIRValue *ptr, String name) {
  UIRValue inst = {.kind = UIRValueKind::Load};
  inst.load.ptr = ptr;
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildStore(UIRValue *value, UIRValue *ptr) {
  UIRValue inst = {.kind = UIRValueKind::Store};
  inst.store = {.value = value, .ptr = ptr};
  return this->insert(inst);
}

UIRValue *UIRBuilder::buildArg(UIRValue *type, String name) {
  UIRValue inst = {.kind = UIRValueKind::Arg};
  inst.arg.type = type;
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildBinOp(UIRValue *lhs, UIRValue *rhs, UIROpcode opcode,
                                 String name) {
  UIRValue inst = {.kind = UIRValueKind::BinOp};
  inst.binop = {.opcode = opcode, .lhs = lhs, .rhs = rhs};
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildUnaryOp(UIRValue *value, UIROpcode opcode,
                                   String name) {
  UIRValue inst = {.kind = UIRValueKind::UnaryOp};
  inst.unaryop = {.opcode = opcode, .value = value};
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildCall(UIRValue *callee, Slice<UIRValue *> arguments,
                                Option<UIRValue *> receiver, String name) {
  UIRValue inst = {.kind = UIRValueKind::Call};
  inst.call = {.callee = callee, .arguments = arguments, .receiver = receiver};
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildIndex(UIRValue *ptr, UIRValue *index, String name) {
  UIRValue inst = {.kind = UIRValueKind::Index};
  inst.index = {.ptr = ptr, .index = index};
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildRange(UIRValue *ptr, UIRValue *start, UIRValue *end,
                                 String name) {
  UIRValue inst = {.kind = UIRValueKind::Range};
  inst.range = {ptr, start, end};
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildLookupPtr(UIRValue *parent, String member,
                                     String name) {
  UIRValue inst = {.kind = UIRValueKind::LookupPtr};
  inst.lookup.parent = parent;
  inst.lookup.member = member;
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildLookupValue(UIRValue *parent, String member,
                                       String name) {
  UIRValue inst = {.kind = UIRValueKind::LookupValue};
  inst.lookup.parent = parent;
  inst.lookup.member = member;
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildAggregate(UIRValue *type, Slice<String> names,
                                     Slice<UIRValue *> values, String name) {
  UIRValue inst = {.kind = UIRValueKind::Aggregate};
  inst.aggregate = {.type = type, .names = names, .values = values};
  return this->insert(inst, false, name);
}

// If `value` is null then this returns `void`
UIRValue *UIRBuilder::buildReturn(Option<UIRValue *> value) {
  UIRValue inst = {.kind = UIRValueKind::Return};
  inst.ret = {.value = value};
  return this->insert(inst);
}

UIRValue *UIRBuilder::buildBr(UIRBlock *block) {
  UIRValue inst = {.kind = UIRValueKind::Branch};
  inst.br = block;
  return this->insert(inst);
}

UIRValue *UIRBuilder::buildCondBr(UIRValue *condition, UIRBlock *then,
                                  UIRBlock *_else) {
  UIRValue inst = {.kind = UIRValueKind::CondBranch};
  inst.condbr = {.condition = condition, .then = then, ._else = _else};
  return this->insert(inst);
}

UIRValue *UIRBuilder::buildSwitch(UIRValue *value, UIRBlock *default_block,
                                  size_t cases) {
  UIRValue inst = {.kind = UIRValueKind::Switch};
  inst._switch.condition = value;
  inst._switch.default_block = default_block;

  uint8_t *onval_ptr =
      this->module->allocator->allocZeroed(sizeof(void *) * cases);
  uint8_t *blocks_ptr =
      this->module->allocator->allocZeroed(sizeof(void *) * cases);
  inst._switch.onvals = {.ptr = (UIRValue **)onval_ptr, .len = cases};
  inst._switch.blocks = {.ptr = (UIRBlock **)blocks_ptr, .len = cases};
  inst._switch.slots = 0;

  return this->insert(inst);
}

void UIRBuilder::addCase(UIRValue *switch_inst, UIRValue *onval,
                         UIRBlock *then) {
  assert(switch_inst->kind == UIRValueKind::Switch &&
         "Cannot add switch case to non-switch instruction");
  assert(switch_inst->_switch.slots < switch_inst->_switch.onvals.len &&
         "Switch instruction is already full");

  switch_inst->_switch.onvals[switch_inst->_switch.slots] = onval;
  switch_inst->_switch.blocks[switch_inst->_switch.slots] = then;
  switch_inst->_switch.slots += 1;
}

UIRValue *UIRBuilder::buildComptime(String name) {
  UIRValue inst = {.kind = UIRValueKind::Comptime};
  return this->insert(inst, false, name);
}
UIRValue *UIRBuilder::buildTypeOf(UIRValue *value, String name) {
  UIRValue inst = {.kind = UIRValueKind::TypeOf};
  inst._typeof = value;
  return this->insert(inst, true, name);
}

UIRValue *UIRBuilder::buildGlobalVariable(Option<UIRValue *> type,
                                          Option<UIRValue *> constant,
                                          String name) {
  UIRValue inst = {.kind = UIRValueKind::GlobalVariable};
  inst.global_variable = {type, constant};
  return this->insert(inst, true, name);
}

UIRValue *UIRBuilder::buildFunction(Slice<UIRValue *> parameters,
                                    UIRValue *return_type, String name) {
  UIRValue inst = {.kind = UIRValueKind::Function};
  inst.function = {
      .parameter_types = parameters,
      .return_type = return_type,
  };
  return this->insert(inst, true, name);
}

UIRValue *UIRBuilder::buildPointer(UIRValue *child_type, String name) {
  UIRValue inst = {.kind = UIRValueKind::Pointer};
  inst.pointer = child_type;
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildSlice(UIRValue *element, UIRValue *length,
                                 bool is_pointer, String name) {
  UIRValue inst = {.kind = UIRValueKind::Slice};
  inst.slice = {.element = element, .length = length, .is_pointer = is_pointer};
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildStruct(Slice<UIRStruct::Field> fields, String name) {
  UIRValue inst = {.kind = UIRValueKind::Struct};
  inst._struct.fields = fields;
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildEnum(UIRValue *repr_type,
                                Slice<UIREnum::Member> members, String name) {
  UIRValue inst = {.kind = UIRValueKind::Enum};
  inst._enum.repr_type = repr_type;
  inst._enum.members = members;
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildUnion(UIRValue *repr_type,
                                 Slice<UIRStruct::Field> variants,
                                 String name) {
  UIRValue inst = {.kind = UIRValueKind::Union};
  inst._union.repr_type = repr_type;
  inst._union.variants = variants;
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildNamespace(String name) {
  UIRValue inst = {.kind = UIRValueKind::Namespace};
  return this->insert(inst, false, name);
}
