#pragma once

#include "literal.hpp"
#include "mir.hpp"

void printInst(MIRValue *inst);   // Forward Declaration
void printBlock(MIRBlock *block); // Forward Declaration

void printLiteral(MIRLiteral *literal) {
  switch (literal->kind) {
  case MIRLiteralKind::Null: {
    std::cout << "null";
    break;
  }
  case MIRLiteralKind::Bool: {
    std::cout << literal->_bool;
    break;
  }
  case MIRLiteralKind::Integer: {
    std::cout << literal->_int;
    break;
  }
  case MIRLiteralKind::Float: {
    std::cout << literal->_float;
    break;
  }
  case MIRLiteralKind::Pointer: {
    std::cout << literal->pointer;
    break;
  }
  case MIRLiteralKind::Slice:
  case MIRLiteralKind::SIMD: {
    std::cout << "[" << literal->slice.len << "]" << literal->slice.pointer;
    break;
  }
  case MIRLiteralKind::TypeId: {
    std::cout << "TypeId";
    break;
  }
  case MIRLiteralKind::Function: {
    std::cout << literal->function_entry;
    break;
  }
  }
}

void printOpcode(MIROpcode opcode) {
  // TODO: print opcode
}

void printRef(MIRValue *value) {
  if (value == nullptr) {
    std::cout << "null";
  } else if (value->kind == MIRValueKind::Literal) {
    printLiteral(&value->literal);
  } else if (value->kind == MIRValueKind::Function) {
    printInst(value);
  } else {
    std::cout << "%" << value;
  }
}

void printValue(MIRValue *value) {}

void printInst(MIRValue *inst) {
  switch (inst->kind) {
  case MIRValueKind::Nop: {
    break;
  }
  case MIRValueKind::Field: {
    std::cout << "%" << inst << " = field `";
    printRef(inst->field.type);
    std::cout << "`, ";
    printRef(inst->field.initial);
    break;
  }
  case MIRValueKind::Load: {
    std::cout << "%" << inst << " = load ";
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
  case MIRValueKind::BinOp: {
    std::cout << "%" << inst << " = ";
    printOpcode(inst->binop.opcode);
    std::cout << " ";
    printRef(inst->binop.lhs);
    std::cout << ", ";
    printRef(inst->binop.rhs);
    break;
  }
  case MIRValueKind::UnaryOp: {
    std::cout << "%" << inst << " = ";
    printOpcode(inst->unaryop.opcode);
    std::cout << " ";
    printRef(inst->unaryop.value);
    break;
  }
  case MIRValueKind::GEP: {
    std::cout << "%" << inst << " = gep %" << inst->gep.ptr << ", %"
              << inst->gep.index;
    break;
  }
  case MIRValueKind::Call: {
    std::cout << "%" << inst << " = call %" << inst->call.callee << "(";
    for (size_t i = 0; i < inst->call.arguments.len; i++) {
      if (i != 0) {
        std::cout << ", ";
      }

      printRef(inst->call.arguments.ptr[i]);
    }
    std::cout << ")";
    break;
  }
  case MIRValueKind::Return: {
    std::cout << "ret";
    if (inst->ret.value != nullptr) {
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
