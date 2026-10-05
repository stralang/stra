#include "common/debug.hpp"
#include "define.hpp"
#include "rir/rir.hpp"
#include "llvm-c/Types.h"
#include <cstring>
#include <llvm-c/Core.h>
#include <sstream>

LLVMValueRef genAssembly(CodeGenModule *codegen, LLVMBuilderRef builder,
                         RIRValue *inst) {
  ArrayList<LLVMValueRef> inputs;
  ArrayList<LLVMTypeRef> input_types;
  std::ostringstream assembly;
  std::ostringstream constraints;
  std::ostringstream clobbered;

  inputs.init(codegen->allocator, 8);
  input_types.init(codegen->allocator, 8);

  for (size_t i = 0; i < inst->assembly.len; i++) {
    if (i != 0) {
      assembly << "\n";
    }

    RIRAssembly *rir_inst = inst->assembly.ptr + i;
    assembly << rir_inst->name;

    for (size_t o = 0; o < rir_inst->operands.len; o++) {
      RIRAssembly::Operand *operand = rir_inst->operands.ptr + o;
      if (o != 0) {
        assembly << ",";
      }

      if (operand->kind == RIRAssembly::Operand::Register) {
        assembly << " %" << operand->reg;
        clobbered << ",~{" << operand->reg << "}";
        continue;
      }

      LLVMValueRef value = getReference(codegen, operand->rir_inst);

      if (inputs.length != 0) {
        constraints << ",";
      }

      LLVMValueRef operand_value = getReference(codegen, operand->rir_inst);
      if (operand->kind == RIRAssembly::Operand::Return) {
        constraints << "=*m";
      } else {
        constraints << "r";
      }

      inputs.push(operand_value);
      input_types.push(LLVMTypeOf(operand_value));
      assembly << " $" << (inputs.length - 1);
    }
  }

  std::string assembly_str = assembly.str();
  std::string constraints_str = constraints.str();

  // Generate
  LLVMTypeRef call_result = LLVMVoidTypeInContext(codegen->ctx);
  LLVMTypeRef func_ty = LLVMFunctionType(call_result, input_types.data.ptr,
                                         input_types.length, false);
  LLVMValueRef inline_asm = LLVMGetInlineAsm(
      func_ty, assembly_str.data(), assembly_str.size(), constraints_str.data(),
      constraints_str.size(), true, true, LLVMInlineAsmDialectATT, false);
  LLVMValueRef call_inst = LLVMBuildCall2(builder, func_ty, inline_asm,
                                          inputs.data.ptr, inputs.length, "");

  size_t sites = 0;
  for (size_t i = 0; i < inst->assembly.len; i++) {
    for (size_t o = 0; o < inst->assembly.ptr[i].operands.len; o++) {
      RIRAssembly::Operand *operand = inst->assembly.ptr[i].operands.ptr + o;
      if (operand->kind == RIRAssembly::Operand::Register) {
        continue;
      }

      sites += 1;
      if (operand->kind == RIRAssembly::Operand::Input) {
        continue;
      }

      RIRValue *rir_value = codegen->rir_context->getInst(operand->rir_inst);
      unsigned elementtypeKind =
          LLVMGetEnumAttributeKindForName("elementtype", strlen("elementtype"));

      RIRType *ptr_type = codegen->rir_context->getType(rir_value->result);
      LLVMTypeRef elemTy = typeToLLVM(codegen, ptr_type->child);
      LLVMAttributeRef attr =
          LLVMCreateTypeAttribute(codegen->ctx, elementtypeKind, elemTy);

      LLVMAddCallSiteAttribute(call_inst, sites, attr);
    }
  }

  return call_inst;
}
