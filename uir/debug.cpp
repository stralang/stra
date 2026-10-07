#include "debug.hpp"

#include "common/debug.hpp"
#include "uir/literal.hpp"
#include "uir/uir.hpp"
#include <ostream>
#include <string>

std::ostream &operator<<(std::ostream &os, const UIRValueId &inst_id) {
  return os << "%" << inst_id.module << "." << inst_id.local;
}

std::ostream &operator<<(std::ostream &os, const UIRBlockId &block_id) {
  return os << "@" << block_id.module << "." << block_id.local;
}

std::ostream &operator<<(std::ostream &os, const UIROpcode &opcode) {
  switch (opcode) {
  case UIROpcode::Add: {
    return os << "add";
  }
  case UIROpcode::Sub: {
    return os << "sub";
  }
  case UIROpcode::Mul: {
    return os << "mul";
  }
  case UIROpcode::Div: {
    return os << "div";
  }
  case UIROpcode::Mod: {
    return os << "mod";
  }
  case UIROpcode::Or: {
    return os << "or";
  }
  case UIROpcode::Xor: {
    return os << "xor";
  }
  case UIROpcode::And: {
    return os << "and";
  }
  case UIROpcode::LeftShift: {
    return os << "shl";
  }
  case UIROpcode::RightShift: {
    return os << "shr";
  }
  case UIROpcode::EqualTo: {
    return os << "eql";
  }
  case UIROpcode::NotEqualTo: {
    return os << "neq";
  }
  case UIROpcode::LessThen: {
    return os << "lt";
  }
  case UIROpcode::GreaterThen: {
    return os << "gt";
  }
  case UIROpcode::LessThenOrEqualTo: {
    return os << "leq";
  }
  case UIROpcode::GreaterThenOrEqualTo: {
    return os << "geq";
  }
  case UIROpcode::As: {
    return os << "as";
  }
  case UIROpcode::Bitcast: {
    return os << "bitcast";
  }
  case UIROpcode::Minus: {
    return os << "minus";
  }
  case UIROpcode::LogicalNot: {
    return os << "lognot";
  }
  case UIROpcode::BitwiseNot: {
    return os << "bitnot";
  }
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const UIRRawData &raw_data) {
  switch (raw_data.kind) {
  case UIRRawDataKind::Void: {
    return os << "<void>";
  }
  case UIRRawDataKind::Bool: {
    return os << raw_data._bool;
  }
  case UIRRawDataKind::Int: {
    return os << raw_data._int;
  }
  case UIRRawDataKind::Float: {
    return os << raw_data._float;
  }
  case UIRRawDataKind::Pointer: {
    assert(0 && "TODO: debug print pointer");
    break;
  }
  case UIRRawDataKind::Slice: {
    os << "{";
    for (size_t i = 0; i < raw_data.values.len; i++) {
      if (i > 0) {
        os << ", ";
      }
      os << raw_data.values.ptr[i];
    }
    return os << "}";
  }
  case UIRRawDataKind::TypeId: {
    return os << "#" << raw_data._typeid;
  }
  case UIRRawDataKind::Namespace: {
    return os << raw_data._namespace;
  }
  }
  return os;
}

std::ostream &printModule(std::ostream &os, UIRContext *ctx,
                          UIRModule *module) {
  return printScope(os, ctx, ctx->getScope(module->definitions), "");
}

std::ostream &printInst(std::ostream &os, UIRContext *ctx, UIRValue *inst,
                        std::string indent) {
  os << indent << inst->id;
  if (inst->name.ptr != nullptr) {
    os << "\"" << inst->name << "\"";
  }
  os << " = ";

  switch (inst->kind) {
  case UIRValueKind::Nop: {
    break;
  }
  case UIRValueKind::LocalVariable: {
    os << "localvar `" << inst->local_variable.type << "`";
    break;
  }
  case UIRValueKind::Load: {
    os << "load " << inst->load.ptr;
    break;
  }
  case UIRValueKind::Store: {
    os << "store " << inst->store.value << ", " << inst->store.ptr;
    break;
  }
  case UIRValueKind::Arg: {
    os << "arg `" << inst->arg.type << "`";
    break;
  }
  case UIRValueKind::BinOp: {
    os << inst->binop.opcode << " " << inst->binop.lhs << ", "
       << inst->binop.rhs;
    break;
  }
  case UIRValueKind::UnaryOp: {
    os << inst->unaryop.opcode << " " << inst->unaryop.value;
    break;
  }
  case UIRValueKind::Index: {
    os << "index " << inst->index.ptr << ", " << inst->index.index;
    break;
  }
  case UIRValueKind::Range: {
    os << "range " << inst->range.ptr << ", " << inst->range.start << " .. "
       << inst->range.end;
    break;
  }
  case UIRValueKind::LookupPtr: {
    os << "lookup_ptr " << inst->lookup.parent;
    os << " \"";
    os.write((const char *)inst->lookup.member.ptr, inst->lookup.member.len);
    os << "\"";
    break;
  }
  case UIRValueKind::LookupValue: {
    os << "lookup_value " << inst->lookup.parent;
    os << " \"";
    os.write((const char *)inst->lookup.member.ptr, inst->lookup.member.len);
    os << "\"";
    break;
  }
  case UIRValueKind::Aggregate: {
    os << "aggregate " << inst->aggregate.type;
    os << " { ";
    for (size_t i = 0; i < inst->aggregate.values.len; i++) {
      if (i != 0) {
        os << ", ";
      }

      if (inst->aggregate.names.ptr != nullptr) {
        os << "\"" << inst->aggregate.names.ptr[i] << "\" = ";
      }
      os << inst->aggregate.values.ptr[i];
    }
    os << " }";
    break;
  }
  case UIRValueKind::Call: {
    os << "call " << inst->call.callee;
    os << "(";
    for (size_t i = 0; i < inst->call.arguments.len; i++) {
      if (i != 0) {
        os << ", ";
      }

      os << inst->call.arguments.ptr[i];
    }
    os << ")";

    if (inst->call.receiver.isSome()) {
      os << " Receiver: " << inst->call.receiver.get();
    }
    break;
  }
  case UIRValueKind::Return: {
    os << "ret";
    if (inst->ret.value.isSome()) {
      os << ' ' << inst->ret.value.get();
    }
    break;
  }
  case UIRValueKind::Branch: {
    os << "br " << inst->br;
    break;
  }
  case UIRValueKind::CondBranch: {
    os << "condbr " << inst->condbr.condition << ", " << inst->condbr.then
       << ", " << inst->condbr._else;
    break;
  }
  case UIRValueKind::Switch: {
    os << "switch " << inst->_switch.condition;
    os << "[\n";

    for (size_t i = 0; i < inst->_switch.onvals.len; i++) {
      os << indent << "  " << inst->_switch.onvals.ptr[i] << ", "
         << inst->_switch.blocks.ptr[i] << "\n";
    }
    os << indent << "  ]";
    break;
  }
  case UIRValueKind::Assembly: {
    os << "asm { ";
    for (size_t i = 0; i < inst->assembly.len; i++) {
      if (i != 0) {
        os << "; ";
      }

      UIRAssembly *asm_inst = inst->assembly.ptr + i;
      os << '`' << asm_inst->name << "` ";
      for (size_t o = 0; o < asm_inst->operands.len; o++) {
        if (o != 0) {
          os << ", ";
        }

        UIRAssembly::Operand *asm_operand = asm_inst->operands.ptr + o;
        if (asm_operand->kind == UIRAssembly::Operand::Register) {
          os << "`%" << asm_operand->reg << '`';
          continue;
        } else if (asm_operand->kind == UIRAssembly::Operand::Return) {
          os << "=";
        }

        os << asm_operand->uir;
      }
    }
    os << " }";
    break;
  }

  case UIRValueKind::Comptime: {
    os << "comptime {\n";
    for (size_t i = 0; i < inst->comptime.blocks.length; i++) {
      UIRBlockId block_id = inst->comptime.blocks.getUnchecked(i);
      printBlock(os, ctx, ctx->getBlock(block_id), indent);
    }
    os << indent << "}";
    break;
  }
  case UIRValueKind::TypeOf: {
    os << "typeof " << inst->_typeof;
    break;
  }

  case UIRValueKind::GlobalVariable: {
    os << "globalvar `";
    if (inst->global_variable.type.isSome()) {
      os << inst->global_variable.type.get();
    } else {
      os << "INFERRED";
    }
    os << "`, ";
    if (inst->global_variable.constant.isSome()) {
      os << inst->global_variable.constant.get();
    }
    break;
  }
  case UIRValueKind::Function: {
    os << "fn(";
    for (size_t i = 0; i < inst->function.parameter_types.len; i++) {
      if (i != 0) {
        os << ", ";
      }
      os << inst->function.parameter_types.ptr[i];
    }
    os << ") " << inst->function.return_type;
    if (inst->function.blocks.data.ptr != nullptr) {
      os << " {\n";
      for (size_t i = 0; i < inst->function.blocks.length; i++) {
        UIRBlockId block_id = inst->function.blocks.getUnchecked(i);
        printBlock(os, ctx, ctx->getBlock(block_id), indent);
      }
      os << indent << "}";
    }
    break;
  }

  case UIRValueKind::Literal: {
    os << "constant ";
    if (inst->literal.lit_type.isSome()) {
      os << "#" << inst->literal.lit_type.get() << " ";
    }
    os << inst->literal.data;
    break;
  }
  case UIRValueKind::Pointer: {
    os << "pointer " << inst->pointer;
    break;
  }
  case UIRValueKind::Slice: {
    os << "[" << inst->slice.element;
    if (inst->slice.is_pointer) {
      os << " *";
    } else if (inst->slice.length.isSome()) {
      os << " x " << inst->slice.length.get();
    }
    os << "]";
    break;
  }
  case UIRValueKind::Struct: {
    os << "struct {\n";
    for (size_t i = 0; i < inst->_struct.fields.len; i++) {
      UIRStruct::Field *field = inst->_struct.fields.ptr + i;
      os << indent << "  " << field->name << ": `" << field->type << "`\n";
    }

    UIRScope *scope = ctx->getScope(inst->_struct.definitions);
    if (scope->list.length > 0) {
      os << "\n";
      printScope(os, ctx, scope, indent + "  ");
    }
    os << indent << "}";
    break;
  }
  case UIRValueKind::Enum: {
    os << "enum `" << inst->_enum.repr_type;
    os << "` {\n";
    for (size_t i = 0; i < inst->_enum.members.len; i++) {
      UIREnum::Member *member = inst->_enum.members.ptr + i;
      os << indent << "  " << member->name;
      if (member->constant.isSome()) {
        os << ": `" << member->constant.get() << "`";
      }
      os << "\n";
    }

    UIRScope *scope = ctx->getScope(inst->_enum.definitions);
    if (scope->list.length > 0) {
      os << "\n";
      printScope(os, ctx, scope, indent + "  ");
    }
    os << indent << "}";
    break;
  }
  case UIRValueKind::Union: {
    os << "union `" << inst->_union.repr_type << "` {\n";
    for (size_t i = 0; i < inst->_union.variants.len; i++) {
      UIRStruct::Field *field = inst->_union.variants.ptr + i;
      os << indent << "  " << field->name << ": `" << field->type << "`\n";
    }

    UIRScope *scope = ctx->getScope(inst->_union.definitions);
    if (scope->list.length > 0) {
      os << "\n";
      printScope(os, ctx, scope, indent + "  ");
    }
    os << indent << "}";
    break;
  }
  case UIRValueKind::Namespace: {
    os << "namespace {\n";
    printScope(os, ctx, ctx->getScope(inst->_namespace.definitions),
               indent + "  ");
    os << indent << "}";
    break;
  }
  }
  return os << "\n";
}

std::ostream &printBlock(std::ostream &os, UIRContext *ctx, UIRBlock *block,
                         std::string indent) {
  os << indent << block->id << ":\n";
  indent = indent + "  ";
  for (size_t i = 0; i < block->instructions.length; i++) {
    UIRValueId inst_id = block->instructions.getUnchecked(i);
    printInst(os, ctx, ctx->getInst(inst_id), indent);
  }
  return os;
}

std::ostream &printScope(std::ostream &os, UIRContext *ctx, UIRScope *scope,
                         std::string indent) {
  for (size_t i = 0; i < scope->list.length; i++) {
    UIRValueId inst_id = scope->list.getUnchecked(i);
    printInst(os, ctx, ctx->getInst(inst_id), indent);
  }
  return os;
}
