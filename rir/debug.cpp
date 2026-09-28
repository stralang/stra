#include "debug.hpp"
#include "rir.hpp"
#include "rir/constant.hpp"
#include "type.hpp"

std::ostream &operator<<(std::ostream &os, const RIRValueId &inst_id) {
  return os << "%" << inst_id.module << "." << inst_id.local;
}

std::ostream &operator<<(std::ostream &os, const RIRBlockId &block_id) {
  return os << "@" << block_id.module << "." << block_id.local;
}

std::ostream &printTypes(std::ostream &os, RIRTypeContext *ctx) {
  for (size_t i = 0; i < ctx->len(); i++) {
    RIRType *type = ctx->getPtrUnchecked(i);
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
    return os << "add";
  }
  case RIROpcode::Sub: {
    return os << "sub";
  }
  case RIROpcode::Mul: {
    return os << "mul";
  }
  case RIROpcode::Div: {
    return os << "div";
  }
  case RIROpcode::Mod: {
    return os << "mod";
  }
  case RIROpcode::Or: {
    return os << "or";
  }
  case RIROpcode::Xor: {
    return os << "xor";
  }
  case RIROpcode::And: {
    return os << "and";
  }
  case RIROpcode::LeftShift: {
    return os << "shl";
  }
  case RIROpcode::RightShift: {
    return os << "shr";
  }
  case RIROpcode::EqualTo: {
    return os << "eql";
  }
  case RIROpcode::NotEqualTo: {
    return os << "neq";
  }
  case RIROpcode::LessThen: {
    return os << "lt";
  }
  case RIROpcode::GreaterThen: {
    return os << "gt";
  }
  case RIROpcode::LessThenOrEqualTo: {
    return os << "leq";
  }
  case RIROpcode::GreaterThenOrEqualTo: {
    return os << "geq";
  }
  case RIROpcode::As: {
    return os << "as";
  }
  case RIROpcode::Bitcast: {
    return os << "bitcast";
  }
  case RIROpcode::Minus: {
    return os << "minus";
  }
  case RIROpcode::LogicalNot: {
    return os << "lognot";
  }
  case RIROpcode::BitwiseNot: {
    return os << "bitnot";
  }
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const RIRConstant &constant) {
  switch (constant.kind) {
  case RIRConstantKind::Bool: {
    os << constant._bool;
    break;
  }
  case RIRConstantKind::Integer: {
    os << constant.integer;
    break;
  }
  case RIRConstantKind::Float: {
    os << constant._float;
    break;
  }
  case RIRConstantKind::List: {
    os << "{";
    for (size_t i = 0; i < constant.constants.len; i++) {
      if (i > 0) {
        os << ", ";
      }
      os << constant.constants.ptr[i];
    }
    os << "}";
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
  case RIRValueKind::Cast: {
    if (inst->cast.bitcast) {
      std::cout << "bit";
    }
    os << "cast " << inst->cast.value << " to #" << inst->result;
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
    os << "global #" << inst->global_variable.type;
    if (inst->global_variable.constant.isSome()) {
      // FIXME: os << ", " << inst->global_variable.constant.get();
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

  case RIRValueKind::Constant: {
    os << "constant #" << inst->constant.type << " `" << inst->constant.value
       << "`";
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

std::ostream &printType(std::ostream &os, RIRTypeContext *ctx, RIRType *type) {
  switch (type->kind) {
  case RIRTypeKind::Void: {
    return os << "void";
  }
  case RIRTypeKind::Bool: {
    return os << "bool";
  }
  case RIRTypeKind::Integer: {
    if (type->integer.is_signed) {
      os << 'i';
    } else {
      os << 'u';
    }

    if (type->integer.bits < 0) {
      return os << "size";
    }
    return os << type->integer.bits;
  }
  case RIRTypeKind::Float: {
    return os << 'f' << type->float_bits;
  }
  case RIRTypeKind::Pointer: {
    return os << "^#" << type->child;
  }
  case RIRTypeKind::Slice: {
    os << '[';
    if (type->slice.length > 0) {
      os << type->slice.length;
    } else if (type->slice.length < 0) {
      os << '*';
    }
    return os << ']' << type->slice.child;
  }
  case RIRTypeKind::Function: {
    os << "fn(";
    for (size_t i = 0; i < type->function.arguments.len; i++) {
      if (i != 0) {
        os << ", ";
      }
      os << "#" << type->function.arguments.ptr[i];
    }
    return os << ") -> #" << type->function._return;
  }
  case RIRTypeKind::Struct: {
    os << "struct { ";
    for (size_t i = 0; i < type->_struct.fields.len; i++) {
      if (i != 0) {
        os << ", ";
      }
      os << "#" << type->_struct.fields.ptr[i];
    }
    return os << " }";
  }
  case RIRTypeKind::Enum: {
    return os << "enum #" << type->_enum.repr;
  }
  case RIRTypeKind::Union: {
    os << "union #" << type->_union.repr << " { ";
    for (size_t i = 0; i < type->_union.variants.len; i++) {
      if (i != 0) {
        os << ", ";
      }
      os << "#" << type->_union.variants.ptr[i];
    }
    return os << " }";
  }
  case RIRTypeKind::TypeId: {
    return os << "typeid";
  }
  }
  return os;
}
