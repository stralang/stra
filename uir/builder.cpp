#include "builder.hpp"
#include "uir.hpp"

UIRScope *UIRBuilder::createScope() {
  UIRScope raw_scope = {
      .id = {this->module->id, (uint32_t)this->module->scopes.len()},
  };
  raw_scope.list.init(this->module->allocator, 8);

  this->module->scopes.push(raw_scope);
  return this->context->getScope(raw_scope.id);
}

UIRBlock *UIRBuilder::createBlock(String name) {
  UIRBlock raw_block = {
      .id = {this->module->id, (uint32_t)this->module->blocks.len()},
      .name = name,
  };
  raw_block.instructions.init(this->module->allocator, 32);

  this->module->blocks.push(raw_block);
  UIRBlock *block = this->context->getBlock(raw_block.id);
  return block;
}

UIRBlock *UIRBuilder::appendBlock(UIRValueId parent, String name) {
  UIRBlock *block = this->createBlock(name);

  UIRValue *parent_inst = this->context->getInst(parent);
  if (parent_inst->kind == UIRValueKind::Function) {
    block->parent = parent;
    parent_inst->function.blocks.push(block->id);
  } else if (parent_inst->kind == UIRValueKind::Comptime) {
    block->parent = parent;
    parent_inst->comptime.blocks.push(block->id);
  }
  return block;
}

UIRValue *UIRBuilder::insert(UIRValue inst, bool global, String name) {
  inst.id = {this->module->id, (uint32_t)this->module->instructions.len()};
  inst.name = name;

  this->module->instructions.push(inst);
  UIRValue *ptr_inst = this->context->getInst(inst.id);

  *ptr_inst = inst;
  if (this->block.isSome()) {
    UIRBlock *block = this->block.get();
    ptr_inst->parent = block->parent;
    block->instructions.push(ptr_inst->id);
  } else if (this->scope.isSome()) {
    UIRScope *scope = this->scope.get();
    ptr_inst->parent = scope->owner;
    scope->list.push(ptr_inst->id);
  } else {
    std::cerr << "Block or Scope must be provided to insert instruction.\n";
    std::abort();
  }
  return ptr_inst;
}

UIRValue *UIRBuilder::buildLocalVariable(UIRValueId type, String name) {
  UIRValue inst = {.kind = UIRValueKind::LocalVariable};
  inst.local_variable = {type};
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildLoad(UIRValueId ptr, String name) {
  UIRValue inst = {.kind = UIRValueKind::Load};
  inst.load.ptr = ptr;
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildStore(UIRValueId value, UIRValueId ptr) {
  UIRValue inst = {.kind = UIRValueKind::Store};
  inst.store = {.value = value, .ptr = ptr};
  return this->insert(inst);
}

UIRValue *UIRBuilder::buildArg(UIRValueId type, String name) {
  UIRValue inst = {.kind = UIRValueKind::Arg};
  inst.arg.type = type;
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildBinOp(UIRValueId lhs, UIRValueId rhs,
                                 UIROpcode opcode, String name) {
  UIRValue inst = {.kind = UIRValueKind::BinOp};
  inst.binop = {.opcode = opcode, .lhs = lhs, .rhs = rhs};
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildUnaryOp(UIRValueId value, UIROpcode opcode,
                                   String name) {
  UIRValue inst = {.kind = UIRValueKind::UnaryOp};
  inst.unaryop = {.opcode = opcode, .value = value};
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildCall(UIRValueId callee, Slice<UIRValueId> arguments,
                                Option<UIRValueId> receiver, String name) {
  UIRValue inst = {.kind = UIRValueKind::Call};
  inst.call = {.callee = callee, .arguments = arguments, .receiver = receiver};
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildIndex(UIRValueId ptr, UIRValueId index,
                                 String name) {
  UIRValue inst = {.kind = UIRValueKind::Index};
  inst.index = {.ptr = ptr, .index = index};
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildRange(UIRValueId ptr, UIRValueId start,
                                 UIRValueId end, String name) {
  UIRValue inst = {.kind = UIRValueKind::Range};
  inst.range = {ptr, start, end};
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildLookupPtr(UIRValueId parent, String member,
                                     String name) {
  UIRValue inst = {.kind = UIRValueKind::LookupPtr};
  inst.lookup.parent = parent;
  inst.lookup.member = member;
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildLookupValue(UIRValueId parent, String member,
                                       String name) {
  UIRValue inst = {.kind = UIRValueKind::LookupValue};
  inst.lookup.parent = parent;
  inst.lookup.member = member;
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildAggregate(UIRValueId type, Slice<String> names,
                                     Slice<UIRValueId> values, String name) {
  UIRValue inst = {.kind = UIRValueKind::Aggregate};
  inst.aggregate = {.type = type, .names = names, .values = values};
  return this->insert(inst, false, name);
}

// If `value` is null then this returns `void`
UIRValue *UIRBuilder::buildReturn(Option<UIRValueId> value) {
  UIRValue inst = {.kind = UIRValueKind::Return};
  inst.ret = {.value = value};
  return this->insert(inst);
}

UIRValue *UIRBuilder::buildBr(UIRBlockId block) {
  UIRValue inst = {.kind = UIRValueKind::Branch};
  inst.br = block;
  return this->insert(inst);
}

UIRValue *UIRBuilder::buildCondBr(UIRValueId condition, UIRBlockId then,
                                  UIRBlockId _else) {
  UIRValue inst = {.kind = UIRValueKind::CondBranch};
  inst.condbr = {.condition = condition, .then = then, ._else = _else};
  return this->insert(inst);
}

UIRValue *UIRBuilder::buildSwitch(UIRValueId value, UIRBlockId default_block,
                                  size_t cases) {
  UIRValue inst = {.kind = UIRValueKind::Switch};
  inst._switch.condition = value;
  inst._switch.default_block = default_block;

  uint8_t *onval_ptr =
      this->module->allocator->allocZeroed(sizeof(void *) * cases);
  uint8_t *blocks_ptr =
      this->module->allocator->allocZeroed(sizeof(void *) * cases);
  inst._switch.onvals = {.ptr = (UIRValueId *)onval_ptr, .len = cases};
  inst._switch.blocks = {.ptr = (UIRBlockId *)blocks_ptr, .len = cases};
  inst._switch.slots = 0;

  return this->insert(inst);
}

void UIRBuilder::addCase(UIRValue *switch_inst, UIRValueId onval,
                         UIRBlockId then) {
  assert(switch_inst->kind == UIRValueKind::Switch &&
         "Cannot add switch case to non-switch instruction");
  assert(switch_inst->_switch.slots < switch_inst->_switch.onvals.len &&
         "Switch instruction is already full");

  switch_inst->_switch.onvals[switch_inst->_switch.slots] = onval;
  switch_inst->_switch.blocks[switch_inst->_switch.slots] = then;
  switch_inst->_switch.slots += 1;
}

UIRValue *UIRBuilder::buildAssembly(Slice<UIRAssembly> instructions,
                                    String name) {
  UIRValue inst = {.kind = UIRValueKind::Assembly, .assembly = instructions};
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildComptime(String name) {
  UIRValue inst = {.kind = UIRValueKind::Comptime};
  return this->insert(inst, false, name);
}
UIRValue *UIRBuilder::buildTypeOf(UIRValueId value, String name) {
  UIRValue inst = {.kind = UIRValueKind::TypeOf};
  inst._typeof = value;
  return this->insert(inst, true, name);
}

UIRValue *UIRBuilder::buildGlobalVariable(Option<UIRValueId> type,
                                          Option<UIRValueId> constant,
                                          String name) {
  UIRValue inst = {.kind = UIRValueKind::GlobalVariable};
  inst.global_variable = {type, constant};
  return this->insert(inst, true, name);
}

UIRValue *UIRBuilder::buildFunction(Slice<UIRValueId> parameters,
                                    UIRValueId return_type, String name) {
  UIRValue inst = {.kind = UIRValueKind::Function};
  inst.function = {
      .parameter_types = parameters,
      .return_type = return_type,
  };
  return this->insert(inst, true, name);
}

UIRValue *UIRBuilder::buildLiteral(UIRLiteral literal, String name) {
  UIRValue inst = {.kind = UIRValueKind::Literal, .literal = literal};
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildPointer(UIRValueId child_type, String name) {
  UIRValue inst = {.kind = UIRValueKind::Pointer};
  inst.pointer = child_type;
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildSlice(UIRValueId element, Option<UIRValueId> length,
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

UIRValue *UIRBuilder::buildEnum(UIRValueId repr_type,
                                Slice<UIREnum::Member> members, String name) {
  UIRValue inst = {.kind = UIRValueKind::Enum};
  inst._enum.repr_type = repr_type;
  inst._enum.members = members;
  return this->insert(inst, false, name);
}

UIRValue *UIRBuilder::buildUnion(UIRValueId repr_type,
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
