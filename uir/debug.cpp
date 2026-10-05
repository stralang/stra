#include "debug.hpp"

#include "common/debug.hpp"
#include "uir/literal.hpp"
#include "uir/uir.hpp"
#include <ostream>
#include <string>

std::ostream &printName(std::ostream &os, UIRContext *ctx, UIRValue *value) {
  if (value->name.ptr != nullptr) {
    os << value->name << "#";
  }
  return os << value->id;
}

std::ostream &printBlockName(std::ostream &os, UIRContext *ctx,
                             UIRBlock *block) {
  if (block->name.ptr != nullptr) {
    os << block->name << "#";
  }
  return os << block->id;
}

std::ostream &printModule(std::ostream &os, UIRContext *ctx,
                          UIRModule *module) {
  return printScope(os, ctx, module->definitions, "");
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

std::ostream &printInst(std::ostream &os, UIRContext *ctx, UIRValue *inst,
                        std::string indent) {
  os << indent << "%";
  printName(os, ctx, inst);
  os << " = ";

  switch (inst->kind) {
  case UIRValueKind::Nop: {
    break;
  }
  case UIRValueKind::LocalVariable: {
    os << "localvar `";
    printName(os, ctx, inst->local_variable.type);
    os << "`";
    break;
  }
  case UIRValueKind::Load: {
    os << "load ";
    printName(os, ctx, inst->load.ptr);
    break;
  }
  case UIRValueKind::Store: {
    os << "store ";
    printName(os, ctx, inst->store.value);
    os << ", ";
    printName(os, ctx, inst->store.ptr);
    break;
  }
  case UIRValueKind::Arg: {
    os << "arg `";
    printName(os, ctx, inst->arg.type);
    os << "`";
    break;
  }
  case UIRValueKind::BinOp: {
    os << inst->binop.opcode << " ";
    printName(os, ctx, inst->binop.lhs);
    os << ", ";
    printName(os, ctx, inst->binop.rhs);
    break;
  }
  case UIRValueKind::UnaryOp: {
    os << inst->unaryop.opcode << " ";
    printName(os, ctx, inst->unaryop.value);
    break;
  }
  case UIRValueKind::Index: {
    os << "index ";
    printName(os, ctx, inst->index.ptr);
    os << ", ";
    printName(os, ctx, inst->index.index);
    break;
  }
  case UIRValueKind::Range: {
    os << "range ";
    printName(os, ctx, inst->range.ptr);
    os << ", ";
    printName(os, ctx, inst->range.start);
    os << " .. ";
    printName(os, ctx, inst->range.end);
    break;
  }
  case UIRValueKind::LookupPtr: {
    os << "lookup_ptr ";
    printName(os, ctx, inst->lookup.parent);
    os << " \"";
    os.write((const char *)inst->lookup.member.ptr, inst->lookup.member.len);
    os << "\"";
    break;
  }
  case UIRValueKind::LookupValue: {
    os << "lookup_value ";
    printName(os, ctx, inst->lookup.parent);
    os << " \"";
    os.write((const char *)inst->lookup.member.ptr, inst->lookup.member.len);
    os << "\"";
    break;
  }
  case UIRValueKind::Aggregate: {
    os << "aggregate ";
    printName(os, ctx, inst->aggregate.type);
    os << " { ";
    for (size_t i = 0; i < inst->aggregate.values.len; i++) {
      if (i != 0) {
        os << ", ";
      }

      if (inst->aggregate.names.ptr != nullptr) {
        os << "\"" << inst->aggregate.names.ptr[i] << "\" = ";
      }
      printName(os, ctx, inst->aggregate.values.ptr[i]);
    }
    os << " }";
    break;
  }
  case UIRValueKind::Call: {
    os << "call ";
    printName(os, ctx, inst->call.callee);
    os << "(";
    for (size_t i = 0; i < inst->call.arguments.len; i++) {
      if (i != 0) {
        os << ", ";
      }

      printName(os, ctx, inst->call.arguments.ptr[i]);
    }
    os << ")";

    if (inst->call.receiver.isSome()) {
      os << " Receiver: ";
      printName(os, ctx, inst->call.receiver.get());
    }
    break;
  }
  case UIRValueKind::Return: {
    os << "ret";
    if (inst->ret.value.isSome()) {
      os << ' ';
      printName(os, ctx, inst->ret.value.get());
    }
    break;
  }
  case UIRValueKind::Branch: {
    os << "br @";
    printBlockName(os, ctx, inst->br);
    break;
  }
  case UIRValueKind::CondBranch: {
    os << "condbr ";
    printName(os, ctx, inst->condbr.condition);
    os << ", @";
    printBlockName(os, ctx, inst->condbr.then);
    os << ", @";
    printBlockName(os, ctx, inst->condbr._else);
    break;
  }
  case UIRValueKind::Switch: {
    os << "switch ";
    printName(os, ctx, inst->_switch.condition);
    os << "[\n";

    for (size_t i = 0; i < inst->_switch.onvals.len; i++) {
      os << indent << "  ";
      printName(os, ctx, inst->_switch.onvals.ptr[i]);
      os << ", @";
      printBlockName(os, ctx, inst->_switch.blocks.ptr[i]);
      os << "\n";
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

        os << "%";
        printName(os, ctx, asm_operand->uir);
      }
    }
    os << " }";
    break;
  }

  case UIRValueKind::Comptime: {
    os << "comptime {\n";
    for (size_t i = 0; i < inst->comptime.blocks.length; i++) {
      printBlock(os, ctx, inst->comptime.blocks.getUnchecked(i), indent);
    }
    os << indent << "}";
    break;
  }
  case UIRValueKind::TypeOf: {
    os << "typeof ";
    printName(os, ctx, inst->_typeof);
    break;
  }

  case UIRValueKind::GlobalVariable: {
    os << "globalvar `";
    if (inst->global_variable.type.isSome()) {
      printName(os, ctx, inst->global_variable.type.get());
    } else {
      os << "INFERRED";
    }
    os << "`, ";
    if (inst->global_variable.constant.isSome()) {
      printName(os, ctx, inst->global_variable.constant.get());
    }
    break;
  }
  case UIRValueKind::Function: {
    os << "fn(";
    for (size_t i = 0; i < inst->function.parameter_types.len; i++) {
      if (i != 0) {
        os << ", ";
      }
      printName(os, ctx, inst->function.parameter_types.ptr[i]);
    }
    os << ") ";
    printName(os, ctx, inst->function.return_type);
    if (inst->function.blocks.data.ptr != nullptr) {
      os << " {\n";
      printScope(os, ctx, inst->function.globals, indent + "  ");
      for (size_t i = 0; i < inst->function.blocks.length; i++) {
        printBlock(os, ctx, inst->function.blocks.getUnchecked(i), indent);
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
    os << "pointer ";
    printName(os, ctx, inst->pointer);
    break;
  }
  case UIRValueKind::Slice: {
    os << "[";
    printName(os, ctx, inst->slice.element);
    if (inst->slice.is_pointer) {
      os << " *";
    } else if (inst->slice.length != nullptr) {
      os << " x ";
      printName(os, ctx, inst->slice.length);
    }
    os << "]";
    break;
  }
  case UIRValueKind::Struct: {
    os << "struct {\n";
    for (size_t i = 0; i < inst->_struct.fields.len; i++) {
      UIRStruct::Field *field = inst->_struct.fields.ptr + i;
      os << indent << "  " << field->name << ": `%";
      printName(os, ctx, field->type);
      os << "`\n";
    }

    if (inst->_struct.definitions->list.length > 0) {
      os << "\n";
      printScope(os, ctx, inst->_struct.definitions, indent + "  ");
    }
    os << indent << "}";
    break;
  }
  case UIRValueKind::Enum: {
    os << "enum `%";
    printName(os, ctx, inst->_enum.repr_type);
    os << "` {\n";
    for (size_t i = 0; i < inst->_enum.members.len; i++) {
      UIREnum::Member *member = inst->_enum.members.ptr + i;
      os << indent << "  " << member->name;
      if (member->constant != nullptr) {
        os << ": `";
        printName(os, ctx, member->constant);
        os << "`";
      }
      os << "\n";
    }

    if (inst->_enum.definitions->list.length > 0) {
      os << "\n";
      printScope(os, ctx, inst->_enum.definitions, indent + "  ");
    }
    os << indent << "}";
    break;
  }
  case UIRValueKind::Union: {
    os << "union `";
    printName(os, ctx, inst->_union.repr_type);
    os << "` {\n";
    for (size_t i = 0; i < inst->_union.variants.len; i++) {
      UIRStruct::Field *field = inst->_union.variants.ptr + i;
      os << indent << "  " << field->name << ": `";
      printName(os, ctx, field->type);
      os << "`\n";
    }

    if (inst->_union.definitions->list.length > 0) {
      os << "\n";
      printScope(os, ctx, inst->_union.definitions, indent + "  ");
    }
    os << indent << "}";
    break;
  }
  case UIRValueKind::Namespace: {
    os << "namespace {\n";
    printScope(os, ctx, inst->_namespace.definitions, indent + "  ");
    os << indent << "}";
    break;
  }
  }
  return os << "\n";
}

std::ostream &printBlock(std::ostream &os, UIRContext *ctx, UIRBlock *block,
                         std::string indent) {
  os << indent;
  printBlockName(os, ctx, block);
  os << ":\n";
  indent = indent + "  ";
  for (size_t i = 0; i < block->instructions.length; i++) {
    printInst(os, ctx, block->instructions.getUnchecked(i), indent);
  }
  return os;
}

std::ostream &printScope(std::ostream &os, UIRContext *ctx, UIRScope *scope,
                         std::string indent) {
  for (size_t i = 0; i < scope->list.length; i++) {
    printInst(os, ctx, scope->list.getUnchecked(i), indent);
  }
  return os;
}
