#include "codegen.hpp"
#include "../ast.hpp"
#include "../containers.hpp"
#include "../environment.hpp"
#include "../helper.hpp"
#include "../operator.hpp"
#include "../print.hpp"
#include "../symbol.hpp"
#include "../types.hpp"
#include "abi/general.hpp"
#include "define.hpp"
#include "passes.hpp"
#include "llvm-c/Core.h"
#include "llvm-c/Error.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Transforms/PassBuilder.h"
#include "llvm-c/Types.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <llvm-c/TargetMachine.h>
#include <sstream>

void genAssembly(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                 Symbol *scope) {
  ArrayList<LLVMValueRef> inputs;
  ArrayList<LLVMTypeRef> input_types;
  ArrayList<LLVMValueRef> outputs;
  ArrayList<LLVMTypeRef> output_types;
  std::ostringstream assembly;
  std::ostringstream return_constaints;
  std::ostringstream read_constraints;
  std::ostringstream clobbered;

  inputs.init(codegen->allocator, 8);
  input_types.init(codegen->allocator, 8);
  outputs.init(codegen->allocator, 8);
  output_types.init(codegen->allocator, 8);

  // Count
  size_t total_outputs = 0;
  for (size_t i = 0; i < node->assembly.instructions.length; i++) {
    NodeAssembly::Instruction *inst = node->assembly.instructions.data.ptr + i;
    for (size_t a = 0; a < inst->arguments.length; a++) {
      NodeAssembly::Argument *arg = inst->arguments.data.ptr + a;
      if (arg->kind == NodeAssembly::Argument::Return) {
        total_outputs += 1;
      }
    }
  }

  // Convert AST to ASM
  for (size_t i = 0; i < node->assembly.instructions.length; i++) {
    if (i != 0) {
      assembly << "\n";
    }

    NodeAssembly::Instruction *inst = node->assembly.instructions.data.ptr + i;
    assembly << inst->name;

    for (size_t a = 0; a < inst->arguments.length; a++) {
      NodeAssembly::Argument *arg = inst->arguments.data.ptr + a;
      if (a != 0) {
        assembly << ",";
      }

      // Registers
      if (arg->kind == NodeAssembly::Argument::Register) {
        assembly << " %" << arg->reg;
        clobbered << ",~{" << arg->reg << "}";
        continue;
      }

      // Literals
      if (arg->node->kind == NodeKind::Value) {
        assembly << " $$" << arg->node->value.data.integer;
        continue;
      }

      // I/O
      if (inputs.length != 0) {
        read_constraints << ",";
      }

      LLVMValueRef arg_ptr = addr(codegen, builder, arg->node, scope);
      LLVMTypeRef arg_type = typeToLLVM(codegen, arg->node->value.type);

      if (arg->kind == NodeAssembly::Argument::Return) {
        return_constaints << "=r,";
        read_constraints << "r";
        outputs.push(arg_ptr);
        output_types.push(arg_type);
      } else {
        read_constraints << "r";
      }

      LLVMValueRef arg_value = LLVMBuildLoad2(builder, arg_type, arg_ptr, "");
      inputs.push(arg_value);
      input_types.push(arg_type);
      assembly << " $" << (inputs.length - 1 + total_outputs);
    }
  }

  // Prepare
  return_constaints << read_constraints.str() << clobbered.str();

  std::string assembly_str = assembly.str();
  std::string constraints_str = return_constaints.str();

  // Generate
  LLVMTypeRef call_result = nullptr;
  if (outputs.length == 0) {
    call_result = LLVMVoidTypeInContext(codegen->ctx);
  } else if (outputs.length == 1) {
    call_result = output_types.data[0];
  } else {
    call_result = LLVMStructTypeInContext(codegen->ctx, output_types.data.ptr,
                                          output_types.length, false);
  }

  LLVMTypeRef func_ty =
      LLVMFunctionType(call_result, input_types.data.ptr, inputs.length, false);
  LLVMValueRef inline_asm = LLVMGetInlineAsm(
      func_ty, assembly_str.data(), assembly_str.size(), constraints_str.data(),
      constraints_str.size(), true, true, LLVMInlineAsmDialectATT, false);

  LLVMValueRef asm_result = LLVMBuildCall2(builder, func_ty, inline_asm,
                                           inputs.data.ptr, inputs.length, "");

  // Store Result
  if (outputs.length == 1) {
    LLVMBuildStore(builder, asm_result, outputs.data.ptr[0]);
  } else {
    for (size_t i = 0; i < outputs.length; i++) {
      LLVMValueRef result = LLVMBuildExtractValue(builder, asm_result, i, "");
      LLVMBuildStore(builder, result, outputs.data.ptr[i]);
    }
  }
}

LLVMValueRef addr(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                  Symbol *scope) {
  switch (node->kind) {
  case NodeKind::Name: {
    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    LLVMValueRef *value = codegen->node_to_value.get(symbol->node);
    if (value == nullptr) {
      gen(codegen, builder, symbol->node, symbol->parent);
      value = codegen->node_to_value.get(symbol->node);
    }

    if (value == nullptr) {
      std::cerr << node->location << " Couldn't find value `" << node->text
                << "`. Aborting...\n";
      std::abort();
    }

    // TODO: A variable may not be generated for runtime
    return *value;
  }
  case NodeKind::Value: {
    LLVMValueRef *cache = codegen->node_to_value.get(node);
    if (cache != nullptr) {
      return *cache;
    }

    LLVMValueRef result = valueToLLVM(codegen, &node->value);

    LLVMValueRef global = LLVMAddGlobal(codegen->mod, LLVMTypeOf(result), "");
    LLVMSetGlobalConstant(global, true);
    LLVMSetLinkage(global, LLVMPrivateLinkage);
    LLVMSetInitializer(global, result);

    codegen->node_to_value.insert(node, global);
    return global;
  }
  case NodeKind::UnaryOperator: {
    if (node->unary_operator.opcode == UnaryOperator::Dereference) {
      return gen(codegen, builder, node->unary_operator.child, scope);
    }
    break;
  }
  case NodeKind::Operator: {
    if (node->_operator.opcode == Operator::MemberAccess) {
      return genMemberAccess(codegen, builder, node, scope);
    } else if (node->_operator.opcode == Operator::As) {
      return addrCastAs(codegen, builder, node, scope);
    } else if (node->_operator.opcode == Operator::Bitcast) {
      LLVMValueRef val = addr(codegen, builder, node->_operator.lhs, scope);
      LLVMTypeRef dst_llvm_type =
          typeToLLVM(codegen, node->_operator.rhs->value.data.type_value);
      return LLVMBuildBitCast(builder, val, LLVMPointerType(dst_llvm_type, 0),
                              "");
    }
    break;
  }
  case NodeKind::Call: {
    return genCall(codegen, builder, node, scope);
  }
  case NodeKind::Index: {
    LLVMValueRef slice = addr(codegen, builder, node->index.slice, scope);
    Type *slice_type = node->index.slice->value.type;
    LLVMValueRef length = nullptr;
    LLVMValueRef ptr = slice;
    LLVMTypeRef type = nullptr;

    LLVMValueRef indices[2];
    indices[0] = LLVMConstInt(LLVMInt32TypeInContext(codegen->ctx), 0, false);
    if (slice_type->kind == TypeKind::Pointer) {
      // Pointer to slice conversion
      type = typeToLLVM(codegen, slice_type->child);
      ptr = LLVMBuildLoad2(builder, typeToLLVM(codegen, slice_type), slice, "");
    } else if (slice_type->slice.length > 0) {
      // Array (compile-time length)
      type = typeToLLVM(codegen, slice_type->slice.type);

      length = LLVMConstInt(
          LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size),
          slice_type->slice.length, false);
    } else if (slice_type->slice.length == 0) {
      // Slice (runtime length)
      indices[1] = LLVMConstInt(LLVMInt32TypeInContext(codegen->ctx), 1, false);
      length = LLVMBuildGEP2(builder, typeToLLVM(codegen, slice_type), slice,
                             indices, 2, "");
      length = LLVMBuildLoad2(
          builder, LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size),
          length, "");

      indices[1] = indices[0];

      ptr = LLVMBuildGEP2(builder, typeToLLVM(codegen, slice_type), slice,
                          indices, 2, "");
      type = typeToLLVM(codegen, slice_type->slice.type);
      ptr = LLVMBuildLoad2(builder, LLVMPointerType(type, 0), ptr, "");
    } else {
      // Pointer Slice (no length)
      type = typeToLLVM(codegen, slice_type->slice.type);
      ptr = LLVMBuildLoad2(builder, typeToLLVM(codegen, slice_type), slice, "");
    }

    indices[0] = LLVMConstInt(
        LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size), 0, false);

    bool is_range = node->index.index->kind == NodeKind::Range;
    LLVMValueRef start = nullptr;
    LLVMValueRef end = nullptr;
    if (is_range) {
      start = gen(codegen, builder, node->index.index->range.min, scope);
      end = gen(codegen, builder, node->index.index->range.max, scope);
    } else {
      start = gen(codegen, builder, node->index.index, scope);
    }

    // Runtime length check
    if (length != nullptr) {
      LLVMValueRef func =
          codegen->function_stack[codegen->function_stack_len - 1].def;
      LLVMBasicBlockRef fail = LLVMAppendBasicBlockInContext(
          codegen->ctx, func, "bounds_check_fail");
      LLVMBasicBlockRef success = LLVMAppendBasicBlockInContext(
          codegen->ctx, func, "bounds_check_success");

      LLVMValueRef in_bounds =
          LLVMBuildICmp(builder, LLVMIntUGT, length, start, "");
      if (is_range) {
        LLVMIntPredicate op = LLVMIntUGT;
        if (node->index.index->range.mode == NodeRange::LessThan) {
          op = LLVMIntUGE;
        }

        LLVMValueRef end_in_bounds =
            LLVMBuildICmp(builder, op, length, end, "");
        in_bounds = LLVMBuildAnd(builder, in_bounds, end_in_bounds, "");
      }

      LLVMBuildCondBr(builder, in_bounds, success, fail);

      // Generate Fail
      LLVMPositionBuilderAtEnd(builder, fail);

      // TODO: Panic
      // FIXME: As of writing there is no way to handle a panic
      LLVMBuildUnreachable(builder); // this doesn't crash

      // Generate Success
      LLVMPositionBuilderAtEnd(builder, success);
    }

    // Index
    indices[0] = start;
    LLVMValueRef elem_ptr = LLVMBuildGEP2(builder, type, ptr, indices, 1, "");
    if (!is_range) {
      return elem_ptr;
    }

    // Get new slice length
    LLVMValueRef new_length = LLVMBuildSub(builder, end, start, "");
    LLVMTypeRef ptr_ty =
        LLVMIntTypeInContext(codegen->ctx, codegen->pointer_size);
    if (node->index.index->range.mode == NodeRange::EqualTo) {
      new_length =
          LLVMBuildAdd(builder, new_length, LLVMConstInt(ptr_ty, 1, false), "");
    }

    // Create Slice
    LLVMValueRef constants[2];
    constants[0] = LLVMConstNull(LLVMTypeOf(elem_ptr));
    constants[1] = LLVMConstInt(LLVMTypeOf(new_length), 0, false);

    LLVMValueRef new_slice =
        LLVMConstStructInContext(codegen->ctx, constants, 2, false);
    new_slice = LLVMBuildInsertValue(builder, new_slice, elem_ptr, 0, "");
    new_slice = LLVMBuildInsertValue(builder, new_slice, new_length, 1, "");

    LLVMValueRef out_slice =
        BuildAlloca(codegen, builder, LLVMTypeOf(new_slice), "");
    LLVMBuildStore(builder, new_slice, out_slice);
    return out_slice;
  }
  case NodeKind::Import: {
    return addr(codegen, builder, node->import.scope->node, node->import.scope);
  }
  }

  return nullptr;
}

LLVMValueRef gen(CodeGenModule *codegen, LLVMBuilderRef builder, Node *node,
                 Symbol *scope) {
  switch (node->kind) {
  case NodeKind::Compound: {
    Symbol *compound_scope = scope->findSymbolByNode(node);
    bool has_own_scope = compound_scope != nullptr;
    if (!has_own_scope) {
      compound_scope = scope;
    }

    size_t old_defer_len = codegen->defer_stack_len;

    for (size_t i = 0; i < node->children.length; i++) {
      gen(codegen, builder, node->children.data.ptr[i], compound_scope);
    }

    if (has_own_scope) {
      codegen->defer_stack_len = old_defer_len;
    }
    break;
  }
  case NodeKind::Name: {
    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    LLVMValueRef *value = codegen->node_to_value.get(symbol->node);
    if (value == nullptr) {
      gen(codegen, builder, symbol->node, symbol->parent);
      value = codegen->node_to_value.get(symbol->node);
    }

    if (value == nullptr) {
      std::cerr << node->location << " Couldn't find value `" << node->text
                << "`. Aborting...\n";
      std::abort();
    }

    // TODO: A variable may not be generated for runtime
    return LLVMBuildLoad2(
        builder, typeToLLVM(codegen, symbol->node->value.type), *value, "");
  }
  case NodeKind::Value: {
    return valueToLLVM(codegen, &node->value);
  }
  case NodeKind::Field: {
    LLVMValueRef *cache = codegen->node_to_value.get(node);
    if (cache != nullptr) {
      return nullptr;
    }

    // Skip compile-time fields
    if (node->field.comptime) {
      return nullptr;
    }

    // Get Symbol
    Symbol *field_symbol = scope->findSymbolByNode(node);

    // Get Name
    String name = {.ptr = nullptr};
    bool builtin = false;
    if (node->field.attributes != nullptr) {
      builtin = containsAttribute(node->field.attributes, "builtin");

      Node *link_name_node = getAttribute(node->field.attributes, "link_name");
      if (link_name_node != nullptr) {
        if (link_name_node->member.value != nullptr) {
          name = link_name_node->member.value->value.data.text;
        } else {
          name = field_symbol->node->field.name;
        }
      }
    }

    if (name.ptr == nullptr) {
      name = field_symbol->mangleName(codegen->allocator);
    }

    // Generate Value
    if (node->value.type->kind == TypeKind::Function) {
      if (builtin || node->field.initial->function.polymorphic) {
        return nullptr;
      }

      // Build function and set name
      LLVMValueRef func =
          gen(codegen, builder, node->field.initial, field_symbol);

      LLVMSetValueName2(func, (const char *)name.ptr, name.len);
      codegen->node_to_value.insert(node, func);
    } else if (node->value.type->kind == TypeKind::TypeId) {
      // Type
      Type *real_type = node->value.data.type_value;
      if (real_type->kind == TypeKind::Struct) {
        char *c_name = (char *)malloc(sizeof(char) * name.len + 8);
        memcpy(c_name, "struct_", 7);
        memcpy(c_name + 7, (const char *)name.ptr, name.len);
        c_name[name.len + 7] = 0;

        LLVMTypeRef type = typeToLLVM(codegen, real_type, c_name);
        free(c_name);
      }

      // Let the type generate it's children
      gen(codegen, builder, node->field.initial, field_symbol);
    } else if (scope->location_aware) {
      // Local Variable
      LLVMTypeRef type = typeToLLVM(codegen, node->value.type);
      LLVMValueRef alloca = BuildAlloca(codegen, builder, type, "");
      LLVMSetValueName2(alloca, (const char *)name.ptr, name.len);
      codegen->node_to_value.insert(node, alloca);

      LLVMValueRef value = LLVMConstNull(type);
      if (node->value.has_data) {
        value = valueToLLVM(codegen, &node->value);
      } else if (!node->field.undefined && node->field.initial != nullptr) {
        value = gen(codegen, builder, node->field.initial, field_symbol);
      }

      LLVMBuildStore(builder, value, alloca);
    } else {
      // Global Variable
      LLVMTypeRef type = typeToLLVM(codegen, node->value.type);
      LLVMValueRef alloca = LLVMAddGlobal(codegen->mod, type, "");
      LLVMSetValueName2(alloca, (const char *)name.ptr, name.len);
      codegen->node_to_value.insert(node, alloca);

      if (!node->field.undefined &&
          node->location.file_hashcode == codegen->source_path_hashcode) {
        LLVMSetInitializer(alloca, valueToLLVM(codegen, &node->value));
      }
    }
    break;
  }
  case NodeKind::Function: {
    if (node->function.polymorphic) {
      return nullptr;
    }

    LLVMValueRef *cache = codegen->node_to_value.get(node);
    if (cache != nullptr) {
      return *cache;
    }

    LLVMTypeRef type = typeToLLVM(codegen, node->value.type);
    LLVMValueRef func = LLVMAddFunction(codegen->mod, "", type);
    codegen->node_to_value.insert(node, func);

    genFunctionBody(codegen, builder, node, scope, type, func);
    codegen->node_to_value.insert(node, func);
    return func;
  }
  case NodeKind::Struct: {
    Symbol *struct_scope = scope->findSymbolByNode(node);
    for (size_t i = 0; i < node->_struct.body.length; i++) {
      gen(codegen, builder, node->_struct.body.data.ptr[i], struct_scope);
    }
    break;
  }
  case NodeKind::Enum: {
    Symbol *enum_scope = scope->findSymbolByNode(node);
    for (size_t i = 0; i < node->_enum.body.length; i++) {
      gen(codegen, builder, node->_enum.body.data.ptr[i], enum_scope);
    }
    break;
  }
  case NodeKind::Union: {
    Symbol *union_scope = scope->findSymbolByNode(node);
    for (size_t i = 0; i < node->_union.body.length; i++) {
      gen(codegen, builder, node->_union.body.data.ptr[i], union_scope);
    }
    break;
  }
  case NodeKind::Namespace: {
    Symbol *namespace_scope = scope->findSymbolByNode(node);
    for (size_t i = 0; i < node->children.length; i++) {
      gen(codegen, builder, node->children.data.ptr[i], namespace_scope);
    }
    break;
  }
  case NodeKind::Member: {
    return valueToLLVM(codegen, &node->value);
  }
  case NodeKind::Import: {
    return gen(codegen, builder, node->import.scope->node, node->import.scope);
  }
  case NodeKind::Const:
  case NodeKind::Slice: {
    // BLANK
    break;
  }
  case NodeKind::Assignment: {
    return genAssignment(codegen, builder, node, scope);
  }
  case NodeKind::UnaryOperator: {
    return genUnary(codegen, builder, node, scope);
  }
  case NodeKind::Operator: {
    return genBinary(codegen, builder, node, scope);
  }
  case NodeKind::Call: {
    LLVMValueRef ret = genCall(codegen, builder, node, scope);
    if (node->value.type->kind == TypeKind::Void) {
      return nullptr;
    } else {
      return LLVMBuildLoad2(builder, typeToLLVM(codegen, node->value.type), ret,
                            "");
    }
  }
  case NodeKind::Index: {
    LLVMValueRef ptr = addr(codegen, builder, node, scope);
    return LLVMBuildLoad2(builder, typeToLLVM(codegen, node->value.type), ptr,
                          "");
  }
  case NodeKind::Initializer: {
    Node *record = node->initializer.record;
    LLVMTypeRef ty = typeToLLVM(codegen, node->value.type);
    LLVMValueRef agg = LLVMConstNull(ty);

    for (size_t i = 0; i < node->initializer.setters.length; i++) {
      Node *setter = node->initializer.setters.data.ptr[i];
      LLVMValueRef value = nullptr;
      size_t idx = i;

      if (node->initializer.is_list) {
        value = gen(codegen, builder, setter, scope);
      } else {
        Symbol *struct_symbol = record->value.data.type_value->_struct.scope;
        Node *struct_node = struct_symbol->node;

        for (size_t l = 0; l < struct_node->_struct.fields.length; l++) {
          Node *field = struct_node->_struct.fields.data.ptr[l];
          if (field->field.name.compare(setter->member.name)) {
            idx = l;
            value = gen(codegen, builder, setter->member.value, scope);
            break;
          }
        }
      }

      agg = LLVMBuildInsertValue(builder, agg, value, idx, "");
    }

    return agg;
  }
  case NodeKind::Return: {
    injectDefer(codegen, builder, scope, false);

    if (node->child == nullptr) {
      LLVMBuildRetVoid(builder);
    } else {
      FuncStackNode parent_func =
          codegen->function_stack[codegen->function_stack_len - 1];
      LLVMValueRef value = gen(codegen, builder, node->child, scope);

      if (parent_func.ret_ptr != nullptr) {
        LLVMBuildStore(builder, value, parent_func.ret_ptr);
        LLVMBuildRetVoid(builder);
      } else {
        LLVMValueRef val = BuildABICast(builder, value, parent_func.ret_type);
        LLVMBuildRet(builder, val);
      }
    }

    break;
  }
  case NodeKind::If: {
    Symbol *if_scope = scope->findSymbolByNode(node);
    LLVMValueRef parent_function =
        codegen->function_stack[codegen->function_stack_len - 1].def;

    // Blocks
    LLVMBasicBlockRef then_block =
        LLVMAppendBasicBlockInContext(codegen->ctx, parent_function, "if_then");
    LLVMBasicBlockRef else_block = nullptr;
    if (node->_if._else != nullptr) {
      else_block = LLVMAppendBasicBlockInContext(codegen->ctx, parent_function,
                                                 "else_body");
    }

    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(
        codegen->ctx, parent_function, "if_merge");

    size_t old_defer_len = codegen->defer_stack_len;

    // Conditional
    LLVMValueRef condition =
        gen(codegen, builder, node->_if.conditional, if_scope);

    if (else_block != nullptr) {
      LLVMBuildCondBr(builder, condition, then_block, else_block);
    } else {
      LLVMBuildCondBr(builder, condition, then_block, merge_block);
    }

    // Body
    LLVMPositionBuilderAtEnd(builder, then_block);
    gen(codegen, builder, node->_if.body, if_scope);

    LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(builder);
    if (LLVMGetBasicBlockTerminator(insert_block) == nullptr) {
      injectDefer(codegen, builder, if_scope, false);
      LLVMBuildBr(builder, merge_block);
    }
    codegen->defer_stack_len = old_defer_len;

    // Else
    if (else_block != nullptr) {
      LLVMPositionBuilderAtEnd(builder, else_block);

      Symbol *else_scope = scope->findSymbolByNode(node->_if._else);
      if (else_scope == nullptr) {
        else_scope = scope;
      }
      gen(codegen, builder, node->_if._else, scope);

      insert_block = LLVMGetInsertBlock(builder);
      if (LLVMGetBasicBlockTerminator(insert_block) == nullptr) {
        injectDefer(codegen, builder, scope, false);
        LLVMBuildBr(builder, insert_block);
      }

      codegen->defer_stack_len = old_defer_len;
    }

    // Merge
    LLVMPositionBuilderAtEnd(builder, merge_block);
    break;
  }
  case NodeKind::For: {
    Symbol *for_scope = scope->findSymbolByNode(node);
    LLVMValueRef parent_function =
        codegen->function_stack[codegen->function_stack_len - 1].def;

    // Block
    LLVMBasicBlockRef condition_block = LLVMAppendBasicBlockInContext(
        codegen->ctx, parent_function, "for_condition");
    LLVMBasicBlockRef do_block =
        LLVMAppendBasicBlockInContext(codegen->ctx, parent_function, "for_do");
    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(
        codegen->ctx, parent_function, "for_merge");

    LLVMBuildBr(builder, condition_block);

    // Loop Stack
    size_t old_defer_len = codegen->defer_stack_len;
    codegen->loop_stack[codegen->loop_stack_len] = {
        .condition = condition_block, ._do = do_block, .merge = merge_block};
    codegen->loop_defer_boundary[codegen->loop_stack_len] =
        codegen->defer_stack_len;
    codegen->loop_stack_len += 1;

    // Conditional
    LLVMPositionBuilderAtEnd(builder, condition_block);
    LLVMValueRef condition =
        gen(codegen, builder, node->_for.conditional, for_scope);

    LLVMBuildCondBr(builder, condition, do_block, merge_block);

    // Do
    LLVMPositionBuilderAtEnd(builder, do_block);
    gen(codegen, builder, node->_for.body, for_scope);

    LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(builder);
    if (LLVMGetBasicBlockTerminator(insert_block) == nullptr) {
      injectDefer(codegen, builder, for_scope, true);
      LLVMBuildBr(builder, condition_block);
    }

    // Merge
    LLVMPositionBuilderAtEnd(builder, merge_block);
    codegen->loop_stack_len -= 1;
    codegen->defer_stack_len = old_defer_len;
    break;
  }
  case NodeKind::Switch: {
    // Blocks
    LLVMBasicBlockRef merge_block =
        LLVMCreateBasicBlockInContext(codegen->ctx, "switch_merge");

    // Switch
    LLVMValueRef value =
        gen(codegen, builder, node->_switch.conditional, scope);
    LLVMValueRef _switch = LLVMBuildSwitch(builder, value, merge_block,
                                           node->_switch.cases.length);

    LLVMValueRef parent_function =
        codegen->function_stack[codegen->function_stack_len - 1].def;

    // Cases
    size_t old_defer_len = codegen->defer_stack_len;

    for (size_t i = 0; i < node->_switch.cases.length; i++) {
      Node *_case = node->_switch.cases.data.ptr[i];
      Symbol *case_scope = scope->findSymbolByNode(_case->_case.body);

      // Body
      LLVMBasicBlockRef case_block = LLVMAppendBasicBlockInContext(
          codegen->ctx, parent_function, "switch_case");
      LLVMPositionBuilderAtEnd(builder, case_block);
      gen(codegen, builder, _case->_case.body, case_scope);

      LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(builder);
      if (LLVMGetBasicBlockTerminator(insert_block) == nullptr) {
        injectDefer(codegen, builder, scope, false);
        LLVMBuildBr(builder, merge_block);
      }

      // Add
      LLVMValueRef constant =
          valueToLLVM(codegen, &_case->_case.constant->value);
      LLVMAddCase(_switch, constant, case_block);

      codegen->defer_stack_len = old_defer_len;
    }

    // Merge
    LLVMAppendExistingBasicBlock(parent_function, merge_block);
    LLVMPositionBuilderAtEnd(builder, merge_block);
    break;
  }
  case NodeKind::Break:
  case NodeKind::Continue: {
    injectDefer(codegen, builder, scope, true);

    // TODO: Named Loop
    LoopBlocks blocks = codegen->loop_stack[codegen->loop_stack_len - 1];
    LLVMBasicBlockRef block = blocks.condition;
    if (node->kind == NodeKind::Break) {
      block = blocks.merge;
    }
    LLVMBuildBr(builder, block);
    break;
  }
  case NodeKind::Defer: {
    codegen->defer_stack[codegen->defer_stack_len] = node->child;
    codegen->defer_stack_len += 1;
    break;
  }
  case NodeKind::Assembly: {
    genAssembly(codegen, builder, node, scope);
    break;
  }
  }

  return nullptr;
}

void injectDefer(CodeGenModule *codegen, LLVMBuilderRef builder, Symbol *scope,
                 bool is_loop) {
  size_t defer_boundary = 0;
  if (is_loop) {
    defer_boundary = codegen->loop_defer_boundary[codegen->loop_stack_len - 1];
  } else {
    defer_boundary =
        codegen->function_defer_boundary[codegen->function_stack_len - 1];
  }

  if (codegen->defer_stack_len - defer_boundary > 0) {
    size_t i = codegen->defer_stack_len;
    while (i > defer_boundary) {
      i -= 1;
      gen(codegen, builder, codegen->defer_stack[i], scope);
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

  this->type_to_llvm.init(this->allocator, 32);
  this->node_to_value.init(this->allocator, 32);
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

  // Generate
  gen(this, this->builder, this->ast, this->symbol);

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

  this->type_to_llvm.deinit();
  this->node_to_value.deinit();
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
