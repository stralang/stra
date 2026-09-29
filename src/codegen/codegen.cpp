#include "codegen.hpp"
#include "../environment.hpp"
#include "../print.hpp"
#include "abi/general.hpp"
#include "containers.hpp"
#include "define.hpp"
#include "passes.hpp"
#include "rir/constant.hpp"
#include "uir/literal.hpp"
#include "uir/types.hpp"
#include "uir/uir.hpp"
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

LLVMValueRef getReference(CodeGenModule *codegen, RIRValueId value_id) {
  RIRValue *value = codegen->rir_context->getInst(value_id);
  if (value->kind == RIRValueKind::Constant) {
    return constantToLLVM(codegen, &value->constant.value, value->result);
  }

  return *codegen->inst_to_llvm.get(value_id);
}

void gen(CodeGenModule *codegen, LLVMBuilderRef builder, RIRValueId inst_id) {
  RIRValue *inst = codegen->rir_context->getInst(inst_id);
  LLVMValueRef out = nullptr;

  switch (inst->kind) {
  case RIRValueKind::LocalVariable: {
    LLVMTypeRef ty = typeToLLVM(codegen, inst->local.type);
    out = BuildAlloca(codegen, builder, ty, "");

    // FIXME: LLVMSetValueName2(out, (const char *)inst->name.ptr,
    // inst->name.len);
    break;
  }
  case RIRValueKind::Load: {
    LLVMTypeRef ty = typeToLLVM(codegen, inst->result);
    LLVMValueRef ptr = getReference(codegen, inst->load.ptr);
    out = LLVMBuildLoad2(builder, ty, ptr, "");
    break;
  }
  case RIRValueKind::Store: {
    LLVMValueRef val = getReference(codegen, inst->store.value);
    LLVMValueRef ptr = getReference(codegen, inst->store.ptr);
    LLVMBuildStore(builder, val, ptr);
    break;
  }
  case RIRValueKind::Arg: {
    LLVMTypeRef ty = typeToLLVM(codegen, inst->arg.type);
    out = BuildAlloca(codegen, builder, ty, "");
    // FIXME: LLVMSetValueName2(out, (const char *)inst->name.ptr,
    // inst->name.len);

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
  case RIRValueKind::BinOp: {
    out = genBinary(codegen, builder, inst);
    break;
  }
  case RIRValueKind::UnaryOp: {
    out = genUnary(codegen, builder, inst);
    break;
  }
  case RIRValueKind::Cast: {
    out = genCast(codegen, builder, inst);
    break;
  }
  case RIRValueKind::Call: {
    out = genCall(codegen, builder, inst);
    break;
  }
  case RIRValueKind::Index: {
    LLVMValueRef slice = getReference(codegen, inst->index.ptr);
    RIRValue *slice_inst = codegen->rir_context->getInst(inst->index.ptr);
    RIRType *slice_type = codegen->rir_context->getType(slice_inst->result);

    LLVMValueRef ptr = slice;
    LLVMTypeRef type = typeToLLVM(codegen, slice_type->slice.child);

    bool in_bounds = false;
    LLVMValueRef indices[2];
    indices[0] = LLVMConstInt(LLVMInt32TypeInContext(codegen->ctx), 0, false);
    if (slice_type->slice.length > 0) {
      // Array (compile-time length)
      ptr = LLVMBuildGEP2(builder, typeToLLVM(codegen, slice_type->id), slice,
                          indices, 1, "");
      LLVMSetIsInBounds(ptr, true);
      in_bounds = true;
    } else if (slice_type->slice.length == 0) {
      // Slice (runtime length)
      indices[1] = indices[0];

      ptr = LLVMBuildGEP2(builder, typeToLLVM(codegen, slice_type->id), slice,
                          indices, 2, "");
      LLVMSetIsInBounds(ptr, true);
      ptr = LLVMBuildLoad2(builder, LLVMPointerType(type, 0), ptr, "");
      in_bounds = true;
    } else {
      // Pointer Slice (no length)
      ptr = LLVMBuildLoad2(builder, typeToLLVM(codegen, slice_type->id), slice,
                           "");
    }

    indices[0] = getReference(codegen, inst->index.index);

    // Runtime length check is handled by UIR

    // Index
    LLVMValueRef elem_ptr = LLVMBuildGEP2(builder, type, ptr, indices, 1, "");
    LLVMSetIsInBounds(elem_ptr, in_bounds);
    out = elem_ptr;
    break;
  }
  // FIXME:
  // case RIRValueKind::Range: {
  //   LLVMValueRef slice = getReference(codegen, inst->range.ptr);
  //   Type *slice_type = inst->range.ptr->result_type->child;
  //
  //   LLVMValueRef length;
  //   LLVMValueRef ptr = slice;
  //   LLVMTypeRef elem_type;
  //
  //   LLVMValueRef indices[2];
  //   indices[0] = LLVMConstInt(LLVMInt32TypeInContext(codegen->ctx), 0,
  //   false); if (slice_type->kind == TypeKind::Pointer) {
  //     // Pointer to slice conversion
  //     elem_type = typeToLLVM(codegen, slice_type->child);
  //     ptr = LLVMBuildLoad2(builder, typeToLLVM(codegen, slice_type), slice,
  //     "");
  //   } else if (slice_type->slice.length > 0) {
  //     // Array (compile-time length)
  //     elem_type = typeToLLVM(codegen, slice_type->slice.type);
  //
  //     length = LLVMConstInt(
  //         LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size),
  //         slice_type->slice.length, false);
  //   } else if (slice_type->slice.length == 0) {
  //     // Slice (runtime length)
  //     indices[1] = LLVMConstInt(LLVMInt32TypeInContext(codegen->ctx), 1,
  //     false); length = LLVMBuildGEP2(builder, typeToLLVM(codegen,
  //     slice_type), slice,
  //                            indices, 2, "");
  //     length = LLVMBuildLoad2(
  //         builder, LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size),
  //         length, "");
  //
  //     indices[1] = indices[0];
  //     ptr = LLVMBuildGEP2(builder, typeToLLVM(codegen, slice_type), slice,
  //                         indices, 2, "");
  //     elem_type = typeToLLVM(codegen, slice_type->slice.type);
  //     ptr = LLVMBuildLoad2(builder, LLVMPointerType(elem_type, 0), ptr, "");
  //   } else {
  //     // Pointer Slice (no length)
  //     elem_type = typeToLLVM(codegen, slice_type->slice.type);
  //     ptr = LLVMBuildLoad2(builder, typeToLLVM(codegen, slice_type), slice,
  //     "");
  //   }
  //
  //   LLVMValueRef start = getReference(codegen, inst->range.start);
  //   LLVMValueRef end = getReference(codegen, inst->range.end);
  //   indices[0] = start;
  //
  //   // TODO: Bounds checking
  //
  //   // Create
  //   LLVMValueRef elem_ptr =
  //       LLVMBuildGEP2(builder, elem_type, ptr, indices, 1, "");
  //
  //   // Get new slice length
  //   LLVMValueRef new_length = LLVMBuildSub(builder, end, start, "");
  //   LLVMTypeRef ptr_ty =
  //       LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size);
  //
  //   // Create Slice
  //   LLVMValueRef constants[2];
  //   constants[0] = LLVMConstNull(LLVMTypeOf(elem_ptr));
  //   constants[1] = LLVMConstInt(LLVMTypeOf(new_length), 0, false);
  //
  //   LLVMValueRef new_slice =
  //       LLVMConstStructInContext(codegen->ctx, constants, 2, false);
  //   new_slice = LLVMBuildInsertValue(builder, new_slice, elem_ptr, 0, "");
  //   new_slice = LLVMBuildInsertValue(builder, new_slice, new_length, 1, "");
  //   codegen->inst_to_llvm.insert(inst, new_slice);
  //   break;
  // }
  // FIXME:
  // case RIRValueKind::LookupPtr: {
  //   LLVMValueRef out = genLookupPtr(codegen, builder, inst);
  //   codegen->inst_to_llvm.insert(inst, out);
  //   break;
  // }
  // FIXME:
  // case RIRValueKind::LookupValue: {
  //   LLVMValueRef ptr = genLookupPtr(codegen, builder, inst);
  //   LLVMTypeRef ty = typeToLLVM(codegen, inst->result_type);
  //   LLVMValueRef out = LLVMBuildLoad2(builder, ty, ptr, "");
  //   codegen->inst_to_llvm.insert(inst, out);
  //   break;
  // }
  // FIXME:
  // case RIRValueKind::Aggregate: {
  //   Type *type = inst->result_type;
  //   LLVMTypeRef llvm_type = typeToLLVM(codegen, type);
  //
  //   LLVMValueRef out = LLVMConstNull(llvm_type);
  //   for (size_t i = 0; i < inst->aggregate.values.len; i++) {
  //     UIRValue *value = inst->aggregate.values.ptr[i];
  //     LLVMValueRef llvm_value = getReference(codegen, value);
  //
  //     // Get Index
  //     size_t index = i;
  //     if (type->kind == TypeKind::Struct) {
  //       // NOTE: This lookup should probably be replaced during analysis
  //       String name = inst->aggregate.names.ptr[i];
  //       UIRValue *struct_inst = type->_struct.inst;
  //       for (size_t l = 0; l < struct_inst->_struct.fields.len; l++) {
  //         UIRStruct::Field *field = struct_inst->_struct.fields.ptr + l;
  //         if (!field->name.compare(name)) {
  //           continue;
  //         }
  //
  //         index = l;
  //       }
  //     }
  //
  //     // Insert Value
  //     out = LLVMBuildInsertValue(builder, out, llvm_value, index, "");
  //   }
  //
  //   codegen->inst_to_llvm.insert(inst, out);
  //   break;
  // }
  case RIRValueKind::Return: {
    if (inst->ret.value.isNone()) {
      LLVMBuildRetVoid(builder);
    } else {
      LLVMValueRef value = getReference(codegen, inst->ret.value.get());
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
  case RIRValueKind::Branch: {
    LLVMBasicBlockRef dst = *codegen->block_to_llvm.get(inst->branch);
    LLVMBuildBr(builder, dst);
    break;
  }
  case RIRValueKind::CondBranch: {
    LLVMValueRef condition = getReference(codegen, inst->cond_branch.condition);
    LLVMBasicBlockRef then =
        *codegen->block_to_llvm.get(inst->cond_branch.then);
    LLVMBasicBlockRef _else =
        *codegen->block_to_llvm.get(inst->cond_branch._else);
    LLVMBuildCondBr(builder, condition, then, _else);
    break;
  }
  case RIRValueKind::Switch: {
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

  case RIRValueKind::GlobalVariable: {
    LLVMValueRef *opt_global = codegen->inst_to_llvm.get(inst_id);
    if (opt_global == nullptr) {
      break;
    }

    LLVMValueRef global = *opt_global;
    if (inst->global_variable.constant.isSome()) {
      RIRConstant const_inst = inst->global_variable.constant.get();
      LLVMValueRef val =
          constantToLLVM(codegen, &const_inst, inst->global_variable.type);
      LLVMSetInitializer(global, val);
    }
    break;
  }
  case RIRValueKind::Function: {
    genFunctionBody(codegen, builder, inst);
    break;
  }
  }

  if (out != nullptr) {
    codegen->inst_to_llvm.insert(inst_id, out);
  }
}

void genDeclaration(CodeGenModule *codegen, RIRValueId inst_id) {
  RIRValue *inst = codegen->rir_context->getInst(inst_id);
  switch (inst->kind) {
  case RIRValueKind::GlobalVariable: {
    LLVMTypeRef ty = typeToLLVM(codegen, inst->global_variable.type);
    LLVMValueRef global = LLVMAddGlobal(codegen->mod, ty, "");
    codegen->inst_to_llvm.insert(inst_id, global);

    LLVMSetValueName2(global, (const char *)inst->global_variable.link_name.ptr,
                      inst->global_variable.link_name.len);
    break;
  }
  case RIRValueKind::Function: {
    LLVMTypeRef ty = typeToLLVM(codegen, inst->function.type);
    LLVMValueRef func = LLVMAddFunction(codegen->mod, "", ty);
    codegen->inst_to_llvm.insert(inst_id, func);

    LLVMSetValueName2(func, (const char *)inst->function.link_name.ptr,
                      inst->function.link_name.len);
    break;
  }
  }
}

void CodeGenModule::generate(CodeGenContext *context, bool emit_ir,
                             bool emit_asm, Optimization opt) {
  // Setup State
  char *name =
      (char *)allocator->allocZeroed(sizeof(char) * this->module_name.len + 1);
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
  for (size_t i = 0; i < this->rir_module->roots.length; i++) {
    RIRValueId inst_id = this->rir_module->roots.getUnchecked(i);
    genDeclaration(this, inst_id);
  }

  // Generate Code
  for (size_t i = 0; i < this->rir_module->roots.length; i++) {
    RIRValueId inst_id = this->rir_module->roots.getUnchecked(i);
    gen(this, this->builder, inst_id);
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
      (char *)allocator->allocZeroed(sizeof(char) * this->output_path.len + 1);
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
