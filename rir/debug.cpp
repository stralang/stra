#include "debug.hpp"
#include "rir.hpp"
#include "type.hpp"

std::ostream &operator<<(std::ostream &os, const RIRValueId &inst_id) {
  return os << "%" << inst_id.module << "." << inst_id.local;
}

std::ostream &operator<<(std::ostream &os, const RIRBlockId &block_id) {
  return os << "@" << block_id.module << "." << block_id.local;
}

std::ostream &printTypes(std::ostream &os, RIRContext *ctx) {
  for (size_t i = 0; i < ctx->types.len(); i++) {
    RIRType *type = ctx->types.getPtrUnchecked(i);
    os << "#" << i << " = ";
    printType(os, ctx, type);
    os << "\n";
  }
  return os;
}

std::ostream &printModule(std::ostream &os, RIRContext *ctx,
                          RIRModule *module) {
  os << "ModuleID: " << module->id << "\n\n";
  for (size_t i = 0; i < module->roots.length; i++) {
    RIRValueId id = module->roots.getUnchecked(i);
    printInst(os, ctx, ctx->getInst(id));
    os << "\n";
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const RIROpcode &opcode) {
  switch (opcode) {
  case RIROpcode::Add: {
    os << "add";
    break;
  }
  case RIROpcode::Sub: {
    os << "sub";
    break;
  }
  case RIROpcode::Mul: {
    os << "mul";
    break;
  }
  case RIROpcode::Div: {
    os << "div";
    break;
  }
  case RIROpcode::Mod: {
    os << "mod";
    break;
  }
  case RIROpcode::Or: {
    os << "or";
    break;
  }
  case RIROpcode::Xor: {
    os << "xor";
    break;
  }
  case RIROpcode::And: {
    os << "and";
    break;
  }
  case RIROpcode::LeftShift: {
    os << "shl";
    break;
  }
  case RIROpcode::RightShift: {
    os << "shr";
    break;
  }
  case RIROpcode::EqualTo: {
    os << "eql";
    break;
  }
  case RIROpcode::NotEqualTo: {
    os << "neq";
    break;
  }
  case RIROpcode::LessThen: {
    os << "lt";
    break;
  }
  case RIROpcode::GreaterThen: {
    os << "gt";
    break;
  }
  case RIROpcode::LessThenOrEqualTo: {
    os << "leq";
    break;
  }
  case RIROpcode::GreaterThenOrEqualTo: {
    os << "geq";
    break;
  }
  case RIROpcode::As: {
    os << "as";
    break;
  }
  case RIROpcode::Bitcast: {
    os << "bitcast";
    break;
  }
  case RIROpcode::Minus: {
    os << "minus";
    break;
  }
  case RIROpcode::LogicalNot: {
    os << "lognot";
    break;
  }
  case RIROpcode::BitwiseNot: {
    os << "bitnot";
    break;
  }
  }
  return os;
}

std::ostream &printInst(std::ostream &os, RIRContext *ctx, RIRValue *inst) {
  os << inst->id << " = ";
  switch (inst->kind) {
  case RIRValueKind::LocalVariable: {
    os << "local #" << inst->local.type;
    break;
  }
  case RIRValueKind::Load: {
    os << "load " << inst->load.ptr;
    break;
  }
  case RIRValueKind::Store: {
    os << "store " << inst->store.ptr << ", " << inst->store.value;
    break;
  }
  case RIRValueKind::Arg: {
    os << "arg #" << inst->arg.type;
    break;
  }
  case RIRValueKind::BinOp: {
    os << inst->binop.opcode << " " << inst->binop.lhs << ", "
       << inst->binop.rhs;
    break;
  }
  case RIRValueKind::UnaryOp: {
    os << inst->unaryop.opcode << " " << inst->unaryop.value;
    break;
  }
  case RIRValueKind::Call: {
    os << "call " << inst->call.callee << "(";
    for (size_t i = 0; i < inst->call.arguments.len; i++) {
      if (i != 0) {
        os << ", ";
      }

      os << inst->call.arguments.ptr[i];
    }
    os << ")";
    break;
  }
  case RIRValueKind::GEP: {
    os << "gep " << inst->gep.ptr << ", " << inst->gep.index;
    break;
  }
  case RIRValueKind::Return: {
    os << "ret ";
    if (inst->ret.value.isSome()) {
      os << inst->ret.value.get();
    }
    break;
  }
  case RIRValueKind::Branch: {
    os << "br " << inst->branch;
    break;
  }
  case RIRValueKind::CondBranch: {
    os << "condbr " << inst->cond_branch.condition << ", "
       << inst->cond_branch.then << ", " << inst->cond_branch._else;
    break;
  }
  case RIRValueKind::Switch: {
    os << "switch " << inst->_switch.condition << " [";
    for (size_t i = 0; i < inst->_switch.onvals.len; i++) {
      if (i != 0) {
        os << ", ";
      }

      os << inst->_switch.onvals.ptr[i] << " -> "
         << inst->_switch.blocks.ptr[i];
    }
    os << "]";
    break;
  }
  case RIRValueKind::GlobalVariable: {
    os << "global " << inst->global_variable.type;
    if (inst->global_variable.constant.isSome()) {
      os << ", " << inst->global_variable.constant.get();
    }
    break;
  }
  case RIRValueKind::Function: {
    os << "fn #" << inst->function.type << " {\n";
    for (size_t i = 0; i < inst->function.blocks.length; i++) {
      RIRBlockId id = inst->function.blocks.getUnchecked(i);
      printBlock(os, ctx, ctx->getBlock(id));
    }
    os << "}";
    break;
  }
  }

  return os;
}

std::ostream &printBlock(std::ostream &os, RIRContext *ctx, RIRBlock *block) {
  os << block->id << ":\n";
  for (size_t i = 0; i < block->instructions.length; i++) {
    RIRValueId id = block->instructions.getUnchecked(i);
    os << "\t";
    printInst(os, ctx, ctx->getInst(id));
    os << "\n";
  }
  return os;
}

std::ostream &printType(std::ostream &os, RIRContext *ctx, RIRType *type) {
  switch (type->kind) {
  case RIRTypeKind::Bool: {
    os << "bool";
    break;
  }
  case RIRTypeKind::Integer: {
    if (type->integer.is_signed) {
      os << 'i';
    } else {
      os << 'u';
    }

    if (type->integer.bits < 0) {
      os << "size";
    } else {
      os << type->integer.bits;
    }
    break;
  }
  case RIRTypeKind::Float: {
    os << 'f' << type->float_bits;
    break;
  }
  case RIRTypeKind::Pointer: {
    os << '^' << type->child;
    break;
  }
  case RIRTypeKind::Slice: {
    os << '[';
    if (type->slice.length > 0) {
      os << type->slice.length;
    } else if (type->slice.length < 0) {
      os << '*';
    }
    os << ']' << type->slice.child;
    break;
  }
  case RIRTypeKind::Function: {
    os << "fn(";
    for (size_t i = 0; i < type->function.arguments.len; i++) {
      if (i != 0) {
        os << ", ";
      }
      os << type->function.arguments.ptr[i];
    }
    os << ") -> " << type->function._return;
    break;
  }
  case RIRTypeKind::Struct: {
    os << "struct { ";
    for (size_t i = 0; i < type->_struct.fields.len; i++) {
      if (i != 0) {
        os << ", ";
      }
      os << type->_struct.fields.ptr[i];
    }
    os << " }";
    break;
  }
  case RIRTypeKind::Enum: {
    os << "enum " << type->_enum.repr;
    break;
  }
  case RIRTypeKind::Union: {
    os << "union " << type->_union.repr << " { ";
    for (size_t i = 0; i < type->_union.variants.len; i++) {
      if (i != 0) {
        os << ", ";
      }
      os << type->_union.variants.ptr[i];
    }
    os << " }";
    break;
  }
  }

  return os;
}
