#include "define.hpp"

LLVMValueRef genLookupPtr(CodeGenModule *codegen, LLVMBuilderRef builder,
                          MIRValue *inst) {
  LLVMValueRef ptr = getReference(codegen, inst->lookup.parent);
  Type *parent_type = inst->lookup.parent->result_type->child;

  // Auto dereference
  if (parent_type->kind == TypeKind::Pointer) {
    parent_type = parent_type->child;
    ptr = LLVMBuildLoad2(builder, typeToLLVM(codegen, parent_type), ptr,
                         "auto_deref_intern");
  }

  // Search
  LLVMTypeRef parent_llvm = typeToLLVM(codegen, parent_type);

  LLVMValueRef out = nullptr;
  LLVMValueRef indices[2] = {
      LLVMConstInt(LLVMInt32TypeInContext(codegen->ctx), 0, false)};

  if (parent_type->kind == TypeKind::Slice) {
    if (inst->lookup.member.compare("ptr")) {
      if (parent_type->slice.length > 0) {
        // Compile-time sized
        out = BuildAlloca(codegen, builder, LLVMPointerType(parent_llvm, 0),
                          "tmp_intern");
        LLVMBuildStore(builder, ptr, out);
      } else {
        // Runtime sized
        indices[1] = indices[0];
        out = LLVMBuildGEP2(builder, parent_llvm, ptr, indices, 2, "");
      }
    } else if (inst->lookup.member.compare("len")) {
      if (parent_type->slice.length > 0) {
        // Compile-time sized
        LLVMTypeRef llvm_usize_ty =
            LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size);
        out = BuildAlloca(codegen, builder, llvm_usize_ty, "tmp_intern");
        LLVMBuildStore(
            builder,
            LLVMConstInt(llvm_usize_ty, parent_type->slice.length, false), out);
      } else {
        // Runtime sized
        indices[1] =
            LLVMConstInt(LLVMInt32TypeInContext(codegen->ctx), 1, false);
        out = LLVMBuildGEP2(builder, parent_llvm, ptr, indices, 2, "");
      }
    }
  } else if (parent_type->kind == TypeKind::Struct) {
    MIRValue *struct_inst = parent_type->_struct.inst;
    for (size_t i = 0; i < struct_inst->_struct.fields.len; i++) {
      MIRStruct::Field *field = struct_inst->_struct.fields.ptr + i;
      if (!field->name.compare(inst->lookup.member)) {
        continue;
      }

      indices[1] = LLVMConstInt(LLVMInt32TypeInContext(codegen->ctx), i, false);
      out = LLVMBuildGEP2(builder, parent_llvm, ptr, indices, 2, "");
      break;
    }
  } else if (parent_type->kind == TypeKind::Union) {
    // TODO: variants
    // TODO: Check Tag
    std::cerr << "TODO: Codegen variant lookup\n";
    std::abort();
  }

  return out;
}
