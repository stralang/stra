#include "codegen.hpp"
#include "../environment.hpp"
#include "../print.hpp"
#include "abi/general.hpp"
#include "containers.hpp"
#include "define.hpp"
#include "mir.hpp"
#include "passes.hpp"
#include "types.hpp"
#include "llvm-c/Core.h"
#include "llvm-c/Error.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Transforms/PassBuilder.h"
#include "llvm-c/Types.h"
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <llvm-c/TargetMachine.h>
#include <sstream>

LLVMValueRef getReference(CodeGenModule *codegen, MIRValue *value) {
  if (value->kind == MIRValueKind::Literal) {
    return literalToLLVM(codegen, &value->literal);
  }

  return *codegen->inst_to_llvm.get(value);
}

void gen(CodeGenModule *codegen, LLVMBuilderRef builder, MIRValue *inst) {
  LLVMValueRef out = nullptr;

  switch (inst->kind) {
  case MIRValueKind::Alloca: {
    LLVMTypeRef ty = typeToLLVM(codegen, inst->alloca.type->literal._typeid);
    out = BuildAlloca(codegen, builder, ty, "");

    LLVMSetValueName2(out, (const char *)inst->name.ptr, inst->name.len);
    break;
  }
  case MIRValueKind::Load: {
    LLVMTypeRef ty = typeToLLVM(codegen, inst->result_type);
    LLVMValueRef ptr = getReference(codegen, inst->load.ptr);
    out = LLVMBuildLoad2(builder, ty, ptr, "");
    break;
  }
  case MIRValueKind::Store: {
    LLVMValueRef val = getReference(codegen, inst->store.value);
    LLVMValueRef ptr = getReference(codegen, inst->store.ptr);
    LLVMBuildStore(builder, val, ptr);
    break;
  }
  case MIRValueKind::Arg: {
    LLVMTypeRef ty = typeToLLVM(codegen, inst->arg.type->literal._typeid);
    out = BuildAlloca(codegen, builder, ty, "");
    LLVMSetValueName2(out, (const char *)inst->name.ptr, inst->name.len);

    // Get Parameter
    FnABICache *abi = codegen->fn_abi_cache.get(codegen->parent_function_type);
    size_t arg_index = codegen->function_arg_index;
    ABIArg abi_arg = abi->args.ptr[arg_index];
    if (abi_arg.kind == ABIArgKind::Ignore) {
      break;
    }

    LLVMValueRef val = LLVMGetParam(codegen->parent_function, arg_index);
    if (abi_arg.kind == ABIArgKind::Direct) {
      val = BuildABICast(builder, val, ty);
    } else if (abi_arg.kind == ABIArgKind::Indirect) {
      val = BuildABICast(builder, val, LLVMPointerType(ty, 0));
      val = LLVMBuildLoad2(builder, ty, val, "");
    }

    // Store
    LLVMBuildStore(builder, val, out);
    if (abi_arg.attribute != nullptr) {
      LLVMAddAttributeAtIndex(codegen->parent_function, arg_index,
                              abi_arg.attribute);
    }

    codegen->function_arg_index += 1;
    break;
  }
  case MIRValueKind::BinOp: {
    out = genBinary(codegen, builder, inst);
    break;
  }
  case MIRValueKind::UnaryOp: {
    out = genUnary(codegen, builder, inst);
    break;
  }
  case MIRValueKind::Call: {
    out = genCall(codegen, builder, inst);
    break;
  }
  case MIRValueKind::GEP: {
    // TODO: GEP
    break;
  }
  case MIRValueKind::Return: {
    if (inst->ret.value == nullptr) {
      LLVMBuildRetVoid(builder);
    } else {
      LLVMValueRef value = getReference(codegen, inst->ret.value);
      if (codegen->return_arg != nullptr) {
        LLVMBuildStore(builder, value, codegen->return_arg);
        LLVMBuildRetVoid(builder);
      } else {
        FnABICache *abi =
            codegen->fn_abi_cache.get(codegen->parent_function_type);
        value = BuildABICast(builder, value, abi->return_arg.type);
        LLVMBuildRet(builder, value);
      }
    }
    break;
  }
  case MIRValueKind::Branch: {
    LLVMBasicBlockRef dst = *codegen->block_to_llvm.get(inst->br);
    LLVMBuildBr(builder, dst);
    break;
  }
  case MIRValueKind::CondBranch: {
    LLVMValueRef condition = getReference(codegen, inst->condbr.condition);
    LLVMBasicBlockRef then = *codegen->block_to_llvm.get(inst->condbr.then);
    LLVMBasicBlockRef _else = *codegen->block_to_llvm.get(inst->condbr._else);
    LLVMBuildCondBr(builder, condition, then, _else);
    break;
  }
  case MIRValueKind::Switch: {
    LLVMValueRef condition = getReference(codegen, inst->_switch.condition);
    LLVMBasicBlockRef _else =
        *codegen->block_to_llvm.get(inst->_switch.default_block);

    LLVMValueRef switch_inst =
        LLVMBuildSwitch(builder, condition, _else, inst->_switch.onvals.len);

    for (size_t i = 0; i < inst->_switch.onvals.len; i++) {
      LLVMValueRef on_val = getReference(codegen, inst->_switch.onvals.ptr[i]);
      LLVMBasicBlockRef dst =
          *codegen->block_to_llvm.get(inst->_switch.blocks.ptr[i]);
      LLVMAddCase(switch_inst, on_val, dst);
    }
    break;
  }

  case MIRValueKind::GlobalVariable: {
    LLVMValueRef global = *codegen->inst_to_llvm.get(inst);

    if (inst->global_variable.constant != nullptr) {
      LLVMValueRef val =
          literalToLLVM(codegen, &inst->global_variable.constant->literal);
      LLVMSetInitializer(global, val);
    }
    break;
  }
  case MIRValueKind::Function: {
    genFunctionBody(codegen, builder, inst);
    break;
  }
  }

  if (out != nullptr) {
    codegen->inst_to_llvm.insert(inst, out);
  }
}

void genDefinition(CodeGenModule *codegen, MIRValue *inst) {
  switch (inst->kind) {
  case MIRValueKind::GlobalVariable: {
    LLVMTypeRef ty = typeToLLVM(codegen, inst->result_type->child);
    LLVMValueRef global = LLVMAddGlobal(codegen->mod, ty, "");
    codegen->inst_to_llvm.insert(inst, global);

    LLVMSetValueName2(global, (const char *)inst->name.ptr, inst->name.len);
    break;
  }
  case MIRValueKind::Function: {
    LLVMTypeRef ty = typeToLLVM(codegen, inst->result_type);
    LLVMValueRef func = LLVMAddFunction(codegen->mod, "", ty);
    codegen->inst_to_llvm.insert(inst, func);

    LLVMSetValueName2(func, (const char *)inst->name.ptr, inst->name.len);
    break;
  }
  }
}

void CodeGenModule::generate(CodeGenContext *context, bool emit_ir,
                             bool emit_asm, Optimization opt) {
  // Setup State
  char *name =
      (char *)allocator->alloc(sizeof(char) * this->module_name.len + 1);
  memcpy(name, this->module_name.ptr, this->module_name.len);
  *(name + this->module_name.len) = 0;

  this->inst_to_llvm.init(this->allocator, 256);
  this->block_to_llvm.init(this->allocator, 32);
  this->type_to_llvm.init(this->allocator, 32);
  this->fn_abi_cache.init(this->allocator, 32);
  this->defer_stack_len = 0;
  this->loop_stack_len = 0;
  this->function_stack_len = 0;

  // Setup Module
  this->ctx = context->ctx;
  this->mod = LLVMModuleCreateWithNameInContext(name, this->ctx);
  this->builder = LLVMCreateBuilderInContext(this->ctx);

  // Setup target info
  LLVMSetTarget(this->mod, context->target_triple);
  LLVMSetDataLayout(this->mod, context->data_layout_str);

  // Get Pointer size
  LLVMTypeRef tmp_ptr = LLVMPointerType(LLVMInt1TypeInContext(this->ctx), 0);
  this->pointer_size =
      LLVMSizeOfTypeInBits(LLVMGetModuleDataLayout(this->mod), tmp_ptr);
  this->target_abi = ABIcreateTarget(context->abi);

  // Generate Definitions
  for (size_t i = 0; i < this->mir_module->definitions->list.length; i++) {
    genDefinition(this, this->mir_module->definitions->list.getUnchecked(i));
  }

  // Generate Code
  for (size_t i = 0; i < this->mir_module->definitions->list.length; i++) {
    gen(this, this->builder,
        this->mir_module->definitions->list.getUnchecked(i));
  }

// Optimize
#ifdef LLVM_OPT_AVAILABLE
  if (opt != Optimization::None) {
    LLVMPassBuilderOptionsRef pass_options = LLVMCreatePassBuilderOptions();
    LLVMErrorRef error = LLVMRunPasses(this->mod, LLVM_OPT_MINIMAL,
                                       context->target_machine, pass_options);
    LLVMDisposePassBuilderOptions(pass_options);

    if (error != NULL) {
      char *msg = LLVMGetErrorMessage(error);
      std::cerr << "LLVM Error: " << msg << "\n";
      std::cerr << "Failed to optimize module. Aborting.\n";
      LLVMDisposeErrorMessage(msg);
      std::abort();
    }
  }
#endif // LLVM_OPT_AVAILABLE

  // Cleanup
  char *output_path =
      (char *)allocator->alloc(sizeof(char) * this->output_path.len + 1);
  memcpy(output_path, this->output_path.ptr, this->output_path.len);
  *(output_path + this->output_path.len) = 0;
  char *error = nullptr;
  LLVMBool fail = 0;

  if (emit_ir) {
    fail = LLVMPrintModuleToFile(this->mod, output_path, &error);
  } else {
    LLVMCodeGenFileType file_type =
        emit_asm ? LLVMAssemblyFile : LLVMObjectFile;
    fail = LLVMTargetMachineEmitToFile(context->target_machine, this->mod,
                                       output_path, file_type, &error);
  }

  // Handle Fail
  if (fail) {
    std::cerr << "LLVM Error: " << error << "\n";
    std::cerr << "Failed to write llvm ir bitcode to file. Aborting.\n";
    LLVMDisposeMessage(error);
    std::abort();
  }

  // Cleanup
  LLVMDisposeMessage(error);
  LLVMDisposeBuilder(this->builder);

  this->inst_to_llvm.deinit();
  this->block_to_llvm.deinit();
  this->type_to_llvm.deinit();
  this->fn_abi_cache.deinit();
}

void CodeGenContext::init(Environment *env, String user_target_triple) {
  // Initialize
  LLVMInitializeAllTargetInfos();
  LLVMInitializeAllTargets();
  LLVMInitializeAllTargetMCs();
  LLVMInitializeAllAsmParsers();
  LLVMInitializeAllAsmPrinters();

  // Target Info
  if (user_target_triple.ptr == nullptr) {
    this->target_triple = LLVMGetDefaultTargetTriple();
  } else {
    char *s = (char *)malloc(sizeof(char) * (user_target_triple.len + 1));
    memcpy(s, (const char *)user_target_triple.ptr, user_target_triple.len);
    s[user_target_triple.len] = 0;
    this->target_triple = s;
  }

  LLVMTargetRef target = nullptr;
  char *errors;
  if (LLVMGetTargetFromTriple(this->target_triple, &target, &errors)) {
    std::cerr << "Error getting target for codegen\n";
    std::cerr << errors << "\n";
    return;
  }

  this->target_machine = LLVMCreateTargetMachine(
      target, target_triple, "", "", LLVMCodeGenLevelDefault, LLVMRelocDefault,
      LLVMCodeModelDefault);
  this->target_data = LLVMCreateTargetDataLayout(target_machine);
  this->data_layout_str = LLVMCopyStringRepOfTargetData(target_data);

  // Context
  this->ctx = LLVMContextCreate();
  this->abi = ABI::SystemV_Amd64;

  // Setup environment
  env->target = decodeTargetTriple(this->target_triple);
  env->endianness = LLVMByteOrder(this->target_data) == LLVMLittleEndian
                        ? Endian::Little
                        : Endian::Big;

  env->pointer_size = LLVMSizeOfTypeInBits(
      this->target_data, LLVMPointerType(LLVMVoidTypeInContext(ctx), 0));
}

void CodeGenContext::deinit() {
  LLVMDisposeTargetData(this->target_data);
  LLVMDisposeTargetMachine(this->target_machine);
  LLVMDisposeMessage(this->data_layout_str);
}
