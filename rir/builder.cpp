#include "builder.hpp"
#include "rir.hpp"
#include "rir/constant.hpp"
#include "rir/type.hpp"
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

RIRValueId RIRBuilder::buildLocalVariable(RIRTypeId type) {
  RIRValue inst = {
      .kind = RIRValueKind::LocalVariable,
      .result =
          this->ctx->types->push({.kind = RIRTypeKind::Pointer, .child = type}),
      .local = {.type = type},
  };
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildLoad(RIRValueId ptr, RIRTypeId result) {
  RIRValue inst = {
      .kind = RIRValueKind::Load,
      .result = result,
      .load = {.ptr = ptr},
  };
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildStore(RIRValueId ptr, RIRValueId value) {
  RIRValue inst = {
      .kind = RIRValueKind::Store,
      .result = this->ctx->types->push({.kind = RIRTypeKind::Void}),
      .store = {.ptr = ptr, .value = value},
  };
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildArg(RIRTypeId type) {
  RIRValue inst = {
      .kind = RIRValueKind::Arg,
      .result =
          this->ctx->types->push({.kind = RIRTypeKind::Pointer, .child = type}),
      .arg = {.type = type},
  };
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

RIRValueId RIRBuilder::buildCast(RIRValueId value, RIRTypeId result,
                                 bool bitcast) {
  RIRValue inst = {.kind = RIRValueKind::Cast, .result = result};
  inst.cast = {.value = value, .bitcast = bitcast};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildCall(RIRValueId callee, Slice<RIRValueId> arguments,
                                 RIRTypeId result) {
  RIRValue inst = {.kind = RIRValueKind::Call, .result = result};
  inst.call = {.callee = callee, .arguments = arguments};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildIndex(RIRValueId ptr, RIRValueId index,
                                  RIRTypeId result) {
  RIRValue inst = {.kind = RIRValueKind::Index, .result = result};
  inst.index = {.ptr = ptr, .index = index};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildRange(RIRValueId ptr, RIRValueId offset,
                                  RIRValueId length,
                                  RIRTypeId result_elem_type) {
  RIRTypeId result = this->ctx->types->push({
      .kind = RIRTypeKind::Slice,
      .slice = {0, result_elem_type},
  });

  RIRValue inst = {.kind = RIRValueKind::Range, .result = result};
  inst.range = {.ptr = ptr, .offset = offset, .length = length};
  return this->buildInst(inst);
}

RIRValueId RIRBuilder::buildFieldAt(RIRValueId ptr, RIRValueId index,
                                    RIRTypeId result) {
  RIRValue inst = {.kind = RIRValueKind::FieldAt, .result = result};
  inst.index = {.ptr = ptr, .index = index};
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
                                           Option<RIRConstant> constant,
                                           String link_name) {
  RIRValue inst = {
      .kind = RIRValueKind::GlobalVariable,
      .result =
          this->ctx->types->push({.kind = RIRTypeKind::Pointer, .child = type}),

  };
  inst.global_variable = {
      .type = type,
      .constant = constant,
      .link_name = link_name,
  };
  return this->buildDeclare(inst);
}

RIRValueId RIRBuilder::buildFunction(RIRTypeId type, bool undefined,
                                     String link_name) {
  RIRValue inst = {.kind = RIRValueKind::Function, .result = type};
  inst.function = {
      .type = type, .undefined = undefined, .link_name = link_name};
  if (!undefined) {
    inst.function.blocks.init(this->module->allocator, 32);
  }
  return this->buildDeclare(inst);
}

RIRValueId RIRBuilder::buildConstant(RIRTypeId type, RIRConstant constant) {
  RIRValue inst = {.kind = RIRValueKind::Constant, .result = type};
  inst.constant = {.type = type, .value = constant};
  return this->buildInst(inst);
}

RIRBlockId RIRBuilder::appendBlock(RIRValueId function) {
  RIRBlockId id = {
      .module = this->module->id,
      .local = (uint32_t)this->module->blocks.len(),
  };
  RIRBlock block = {.id = id, .function = function};
  block.instructions.init(this->module->allocator, 32);

  this->module->blocks.push(block);

  // Add to function
  RIRValue *function_ptr = this->ctx->getInst(function);
  function_ptr->function.blocks.push(id);

  return id;
}

void RIRBuilder::addDebugLocation(RIRValueId inst, SrcLoc location) {
  std::cerr << "TODO: Implement debug information. This is not a crash\n";
  // TODO: Implement debug information
}

void RIRBuilder::addDebugName(RIRValueId inst, String name) {
  std::cerr << "TODO: Implement debug information. This is not a crash\n";
  // TODO: Implement debug information
}
