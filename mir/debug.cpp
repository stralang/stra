#include "literal.hpp"
#include "mir.hpp"

void printInst(MIRValue *inst);   // Forward Declaration
void printBlock(MIRBlock *block); // Forward Declaration

void printLiteral(MIRLiteral *literal) {
  switch (literal->kind) {
  case MIRLiteralKind::Null: {
    std::cout << "null";
    return;
  }
  case MIRLiteralKind::Typed: {
    break;
  }
  }

  switch (literal->lit_type->kind) {
  case TypeKind::Bool: {
    std::cout << literal->_bool;
    break;
  }
  case TypeKind::Integer: {
    std::cout << literal->_int;
    break;
  }
  case TypeKind::Float: {
    std::cout << literal->_float;
    break;
  }
  case TypeKind::Pointer: {
    std::cout << literal->pointer;
    break;
  }
  case TypeKind::Slice:
  case TypeKind::SIMD: {
    std::cout << "[" << literal->slice.len << "]" << literal->slice.pointer;
    break;
  }
  case TypeKind::TypeId: {
    std::cout << literal->_typeid;
    break;
  }
  case TypeKind::Function: {
    std::cout << literal->function_entry;
    break;
  }
  }
}

void printOpcode(MIROpcode opcode) {
  switch (opcode) {
  case MIROpcode::Add: {
    std::cout << "add";
    break;
  }
  case MIROpcode::Sub: {
    std::cout << "sub";
    break;
  }
  case MIROpcode::Mul: {
    std::cout << "mul";
    break;
  }
  case MIROpcode::Div: {
    std::cout << "div";
    break;
  }
  case MIROpcode::Mod: {
    std::cout << "mod";
    break;
  }
  case MIROpcode::Or: {
    std::cout << "or";
    break;
  }
  case MIROpcode::Xor: {
    std::cout << "xor";
    break;
  }
  case MIROpcode::And: {
    std::cout << "and";
    break;
  }
  case MIROpcode::LeftShift: {
    std::cout << "shl";
    break;
  }
  case MIROpcode::RightShift: {
    std::cout << "shr";
    break;
  }
  case MIROpcode::EqualTo: {
    std::cout << "eql";
    break;
  }
  case MIROpcode::NotEqualTo: {
    std::cout << "neq";
    break;
  }
  case MIROpcode::LessThen: {
    std::cout << "lt";
    break;
  }
  case MIROpcode::GreaterThen: {
    std::cout << "gt";
    break;
  }
  case MIROpcode::LessThenOrEqualTo: {
    std::cout << "leq";
    break;
  }
  case MIROpcode::GreaterThenOrEqualTo: {
    std::cout << "geq";
    break;
  }
  case MIROpcode::As: {
    std::cout << "as";
    break;
  }
  case MIROpcode::Bitcast: {
    std::cout << "bitcast";
    break;
  }
  case MIROpcode::LogicalNot: {
    std::cout << "lognot";
    break;
  }
  case MIROpcode::BitwiseNot: {
    std::cout << "bitnot";
    break;
  }
  }
}

void printName(MIRValue *value) {
  if (value->name.ptr != nullptr) {
    std::cout << "%";
    std::cout.write((const char *)value->name.ptr, value->name.len);
  } else {
    std::cout << "%" << value->id;
  }
}

void printRef(MIRValue *value) {
  if (value == nullptr) {
    std::cout << "null";
  } else if (value->kind == MIRValueKind::Literal) {
    printLiteral(&value->literal);
  } else if (value->kind == MIRValueKind::Function) {
    printInst(value);
  } else {
    printName(value);
  }
}

void printValue(MIRValue *value) {}

void printInst(MIRValue *inst) {
  switch (inst->kind) {
  case MIRValueKind::Nop: {
    break;
  }
  case MIRValueKind::Field: {
    printName(inst);
    std::cout << " = field `";
    printRef(inst->field.type);
    std::cout << "`, ";
    printRef(inst->field.initial);
    break;
  }
  case MIRValueKind::Load: {
    printName(inst);
    std::cout << " = load ";
    printRef(inst->load.ptr);
    break;
  }
  case MIRValueKind::Store: {
    std::cout << "store ";
    printRef(inst->store.value);
    std::cout << ", ";
    printRef(inst->store.ptr);
    break;
  }
  case MIRValueKind::Arg: {
    printName(inst);
    std::cout << " = arg ";
    printRef(inst->arg.type);
    break;
  }
  case MIRValueKind::BinOp: {
    printName(inst);
    std::cout << " = ";
    printOpcode(inst->binop.opcode);
    std::cout << " ";
    printRef(inst->binop.lhs);
    std::cout << ", ";
    printRef(inst->binop.rhs);
    break;
  }
  case MIRValueKind::UnaryOp: {
    printName(inst);
    std::cout << " = ";
    printOpcode(inst->unaryop.opcode);
    std::cout << " ";
    printRef(inst->unaryop.value);
    break;
  }
  case MIRValueKind::MemberAccess: {
    printName(inst);
    std::cout << " = memberaccess ";
    printRef(inst->member_access.parent);
    std::cout << " \"";
    std::cout.write((const char *)inst->member_access.member.ptr,
                    inst->member_access.member.len);
    std::cout << "\"";
    break;
  }
  case MIRValueKind::GEP: {
    printName(inst);
    std::cout << " = gep %" << inst->gep.ptr << ", %" << inst->gep.index;
    break;
  }
  case MIRValueKind::Call: {
    printName(inst);
    std::cout << " = call %" << inst->call.callee << "(";
    for (size_t i = 0; i < inst->call.arguments.len; i++) {
      if (i != 0) {
        std::cout << ", ";
      }

      printRef(inst->call.arguments.ptr[i]);
    }
    std::cout << ")";

    if (inst->call.receiver != nullptr) {
      std::cout << " Receiver: ";
      printRef(inst->call.receiver);
    }
    break;
  }
  case MIRValueKind::Return: {
    std::cout << "ret";
    if (inst->ret.value != nullptr) {
      std::cout << ' ';
      printRef(inst->ret.value);
    }
    break;
  }
  case MIRValueKind::Branch: {
    std::cout << "br @" << inst->br;
    break;
  }
  case MIRValueKind::CondBranch: {
    std::cout << "condbr ";
    printRef(inst->condbr.condition);
    std::cout << ", @" << inst->condbr.then << ", @" << inst->condbr._else;
    break;
  }
  case MIRValueKind::Switch: {
    std::cout << "switch ";
    printRef(inst->_switch.condition);
    std::cout << "[\n";

    for (size_t i = 0; i < inst->_switch.onvals.len; i++) {
      printRef(inst->_switch.onvals.ptr[i]);
      std::cout << "\n";
    }
    std::cout << "]";
    break;
  }

  case MIRValueKind::Literal: {
    printLiteral(&inst->literal);
    break;
  }
  case MIRValueKind::Function: {
    std::cout << "fn(";
    for (size_t i = 0; i < inst->function.parameter_types.len; i++) {
      if (i != 0) {
        std::cout << ", ";
      }
      printRef(inst->function.parameter_types.ptr[i]);
    }
    std::cout << ") ";
    printRef(inst->function.return_type);
    if (inst->function.blocks.data.ptr != nullptr) {
      std::cout << " {\n";
      for (size_t i = 0; i < inst->function.blocks.length; i++) {
        printBlock(inst->function.blocks.getPtrUnchecked(i));
      }
      std::cout << "}";
    }
    break;
  }
  }

  std::cout << "\n";
}

void printBlock(MIRBlock *block) {
  std::cout << "<block name>:\n"; // TODO: Block Name
  for (size_t i = 0; i < block->instructions.length; i++) {
    std::cout << "  ";
    printInst(block->instructions.getPtrUnchecked(i));
  }
}

void MIRModule::print() {
  for (size_t i = 0; i < this->instructions.length; i++) {
    printInst(this->instructions.getPtrUnchecked(i));
  }
}
