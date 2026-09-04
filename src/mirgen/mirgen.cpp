#include "mirgen.hpp"
#include "../print.hpp"
#include "define.hpp"
#include "literal.hpp"
#include "mir.hpp"
#include <cstdint>
#include <iostream>

MIRValueId genComptime(MIRGen *mirgen, Node *child, Symbol *scope) {
  MIRValueId value = mirgen->builder.buildComptime(str("comptime"));

  Option<MIRBlockId> prev_block = mirgen->builder.block;
  mirgen->builder.block.setSome(
      mirgen->builder.appendBlock(value, str("entry")));

  MIRValueId output = gen(mirgen, child, scope);
  MIRBlock *current_block =
      mirgen->module.getBlock(mirgen->builder.block.get());
  if (!current_block->hasTerminator(&mirgen->module)) {
    mirgen->builder.buildReturn(Option<MIRValueId>::from(output));
  }

  mirgen->builder.block = prev_block;
  return value;
}

MIRValueId addr(MIRGen *mirgen, Node *node, Symbol *scope) {
  switch (node->kind) {
  case NodeKind::Name: {
    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    MIRValueId *value = mirgen->node_to_value.get(symbol->node);
    if (value == nullptr) {
      std::cerr << node->location << " Couldn't find value `" << node->text
                << "`. Aborting...\n";
      std::abort();
    }

    return *value;
  }
  case NodeKind::Operator: {
    if (node->_operator.opcode == Operator::MemberAccess) {
      return addrMemberAccess(mirgen, node, scope);
    }
    break;
  }
  case NodeKind::Index: {
    MIRValueId ptr = addr(mirgen, node->index.slice, scope);
    MIRValueId index = gen(mirgen, node->index.index, scope);

    MIRValueId out = mirgen->builder.buildGEP(ptr, index);
    mirgen->builder.setSourceLocation(out, node->location);
    return out;
  }
  }

  return {0xFFFFFFFF, 0};
}

MIRValueId gen(MIRGen *mirgen, Node *node, Symbol *scope) {
  switch (node->kind) {
  case NodeKind::Compound: {
    for (size_t i = 0; i < node->children.length; i++) {
      gen(mirgen, node->children.getUnchecked(i), scope);
    }
    break;
  }
  case NodeKind::Block: {
    Symbol *block_scope = scope->findSymbolByNode(node);

    for (size_t i = 0; i < node->children.length; i++) {
      gen(mirgen, node->children.getUnchecked(i), block_scope);
    }
    break;
  }
  case NodeKind::Name: {
    Option<MIRValueId> builtin = genBuiltin(mirgen, node->text);
    if (builtin.isSome()) {
      return builtin.get();
    }

    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    MIRValueId *value = mirgen->node_to_value.get(symbol->node);
    if (value == nullptr) {
      std::cerr << node->location << " Couldn't find value `" << node->text
                << "`. Aborting...\n";
      std::abort();
    }

    MIRValueId out = mirgen->builder.buildLoad(*value, {.ptr = nullptr});
    mirgen->builder.setSourceLocation(out, node->location);
    return out;
  }
  case NodeKind::RawString: {
    Type int_t = {.kind = TypeKind::Integer};
    int_t.integer = {.is_untyped = false, .is_signed = false, .bits = 8};

    // Parse text
    uint8_t *real_text = (uint8_t *)mirgen->allocator->alloc(node->text.len);
    size_t len = 0;
    bool escape = false;
    for (size_t i = 0; i < node->text.len; i++) {
      uint8_t c = node->text.ptr[i];
      if (escape) {
        if (c == '0') {
          c = '\0';
        } else if (c == 'n') {
          c = '\n';
        }
        escape = false;
      } else if (c == '\\') {
        escape = true;
        continue;
      }

      real_text[len] = c;
      len += 1;
    }

    // Set Value
    Type slice_t = {.kind = TypeKind::Slice};
    slice_t.slice = SliceType{
        .length = (int64_t)len,
        .type = mirgen->ctx->type_cache->get(int_t),
    };

    MIRLiteral slice_lit = {
        .lit_type = mirgen->ctx->type_cache->get(slice_t),
        .kind = MIRLiteralKind::Typed,
    };
    slice_lit.inline_data = real_text; // Length is stored in the type
    return mirgen->ctx->makeLiteral(slice_lit);
  }
  case NodeKind::Value: {
    return valueToMIR(mirgen, &node->value);
  }
  case NodeKind::Field: {
    // Get Symbol
    Symbol *field_symbol = scope->findSymbolByNode(node);

    // TODO: Add attributes

    // Generate field
    MIRValueId field;
    if (scope->location_aware) {
      Option<MIRValueId> initial;
      MIRValueId type;
      if (node->field.initial != nullptr) {
        initial.setSome(gen(mirgen, node->field.initial, field_symbol));
      }
      if (node->field.type != nullptr) {
        type = genComptime(mirgen, node->field.type, field_symbol);
      } else {
        type = mirgen->builder.buildTypeOf(initial.get(), {.ptr = nullptr});
      }

      field = mirgen->builder.buildAlloca(type, node->field.name);
      mirgen->node_to_value.insert(node, field);

      if (initial.isSome()) {
        mirgen->builder.buildStore(initial.get(), field);
      }
    } else if (node->field.initial != nullptr &&
               node->field.initial->kind == NodeKind::Function) {
      field = gen(mirgen, node->field.initial, field_symbol);
    } else {
      field = *mirgen->node_to_value.get(node); // Pre-generated

      Option<MIRValueId> type;
      Option<MIRValueId> constant;
      if (node->field.type != nullptr) {
        type.setSome(genComptime(mirgen, node->field.type, field_symbol));
      }
      if (node->field.initial != nullptr) {
        constant.setSome(
            genComptime(mirgen, node->field.initial, field_symbol));
      }

      MIRValue *glob = mirgen->module.getInstr(field);
      glob->global_variable.type = type;
      glob->global_variable.constant = constant;
      glob->global_variable.undefined = node->field.undefined;
    }

    mirgen->builder.setSourceLocation(field, node->location);
    return field;
  }
  case NodeKind::Function: {
    Symbol *fn_symbol = scope->findSymbolByNode(node);

    // Parameter Types
    Slice<MIRValueId> parameters = {
        .ptr = (MIRValueId *)mirgen->allocator->alloc(
            sizeof(MIRValueId) * node->function.parameters.length),
        .len = node->function.parameters.length,
    };

    for (size_t i = 0; i < node->function.parameters.length; i++) {
      Node *arg = node->function.parameters.getUnchecked(i);
      parameters.ptr[i] = gen(mirgen, arg->field.type, fn_symbol);
    }

    // Return Type
    Option<MIRValueId> return_type;
    if (node->function.return_type != nullptr) {
      return_type.setSome(gen(mirgen, node->function.return_type, fn_symbol));
    }

    // Build Function
    MIRValueId value;
    MIRValueId *cache = mirgen->node_to_value.get(node);
    if (cache != nullptr) {
      value = *cache; // Pre-generated

      MIRValue *fn_inst = mirgen->module.getInstr(value);
      fn_inst->function.parameter_types = parameters;
      fn_inst->function.return_type = return_type;
    } else {
      value = mirgen->builder.buildFunction(parameters, return_type, str(""));
    }
    mirgen->builder.setSourceLocation(value, node->location);

    // Body
    if (node->function.body != nullptr) {
      MIRValue *fn_inst = mirgen->module.getInstr(value);
      MIRScopeId globals = mirgen->builder.makeScope(value);
      fn_inst->function.globals = globals;

      fn_inst->function.blocks.init(mirgen->allocator, 32);
      MIRBlockId entry_block = mirgen->builder.appendBlock(value, str("entry"));

      Option<MIRBlockId> prev_block = mirgen->builder.block;
      Option<MIRScopeId> prev_scope = mirgen->builder.scope;
      mirgen->builder.block.setSome(entry_block);
      mirgen->builder.scope.setSome(globals);

      // Parameters
      for (size_t i = 0; i < node->function.parameters.length; i++) {
        Node *arg = node->function.parameters.getUnchecked(i);
        MIRValueId mir_arg =
            mirgen->builder.buildArg(parameters.ptr[i], arg->field.name);
        mirgen->builder.setSourceLocation(mir_arg, arg->location);
        mirgen->node_to_value.insert(arg, mir_arg);
      }

      // Body
      gen(mirgen, node->function.body, fn_symbol);

      // End
      mirgen->builder.block = prev_block;
      mirgen->builder.scope = prev_scope;
    } else {
      MIRValue *fn_inst = mirgen->module.getInstr(value);
      fn_inst->function.undefined = node->function.undefined;

      fn_inst->function.parameter_types = parameters;
    }

    return value;
  }
  case NodeKind::Struct: {
    return genStruct(mirgen, node, scope);
  }
  case NodeKind::Enum: {
    return genEnum(mirgen, node, scope);
  }
  case NodeKind::Union: {
    return genUnion(mirgen, node, scope);
  }
  case NodeKind::Namespace: {
    return genNamespace(mirgen, node, scope);
  }
  case NodeKind::Slice: {
    MIRValueId element = gen(mirgen, node->slice.type, scope);
    Option<MIRValueId> length;
    if (node->slice.length != nullptr) {
      length.setSome(gen(mirgen, node->slice.length, scope));
    }

    MIRValueId out = mirgen->builder.buildSlice(
        element, length, node->slice.is_pointer, {.ptr = nullptr});
    mirgen->builder.setSourceLocation(out, node->location);
    return out;
  }
  case NodeKind::Assignment: {
    return genAssignment(mirgen, node, scope);
  }
  case NodeKind::UnaryOperator: {
    return genUnary(mirgen, node, scope);
  }
  case NodeKind::Operator: {
    return genBinary(mirgen, node, scope);
  }
  case NodeKind::Call: {
    MIRValueId callee = addr(mirgen, node->call.callee, scope);
    Slice<MIRValueId> arguments = {
        .ptr = (MIRValueId *)mirgen->allocator->alloc(
            sizeof(MIRValueId) * node->call.arguments.length),
        .len = node->call.arguments.length,
    };

    // Receiver
    Option<MIRValueId> receiver;
    if (node->call.callee->_operator.opcode == Operator::MemberAccess) {
      receiver.setSome(addr(mirgen, node->call.callee->_operator.lhs, scope));
    }

    // Arguments
    for (size_t i = 0; i < arguments.len; i++) {
      arguments.ptr[i] =
          gen(mirgen, node->call.arguments.getUnchecked(i), scope);
    }

    MIRValueId out = mirgen->builder.buildCall(callee, arguments, receiver,
                                               {.ptr = nullptr});
    mirgen->builder.setSourceLocation(out, node->location);
    return out;
  }
  case NodeKind::Index: {
    MIRValueId ptr = addr(mirgen, node, scope);
    return mirgen->builder.buildLoad(ptr);
  }
  case NodeKind::Return: {
    Option<MIRValueId> ret_value;
    if (node->child != nullptr) {
      ret_value.setSome(gen(mirgen, node->child, scope));
    }

    MIRValueId out = mirgen->builder.buildReturn(ret_value);
    mirgen->builder.setSourceLocation(out, node->location);
    return out;
  }
  case NodeKind::If: {
    Symbol *if_scope = scope->findSymbolByNode(node);
    MIRBlock *entry_block =
        mirgen->module.getBlock(mirgen->builder.block.get());
    MIRValueId parent_define = entry_block->parent;

    // Blocks
    MIRBlockId then_block =
        mirgen->builder.appendBlock(parent_define, str("if_then"));
    Option<MIRBlockId> else_block;
    if (node->_if._else != nullptr) {
      else_block.setSome(
          mirgen->builder.appendBlock(parent_define, str("if_else")));
    }

    MIRBlockId merge_block =
        mirgen->builder.appendBlock(parent_define, str("if_merge"));

    // Conditional
    MIRValueId condition = gen(mirgen, node->_if.conditional, scope);

    if (else_block.isSome()) {
      mirgen->builder.buildCondBr(condition, then_block, else_block.get());
    } else {
      mirgen->builder.buildCondBr(condition, then_block, merge_block);
    }

    // Body
    mirgen->builder.block.setSome(then_block);
    gen(mirgen, node->_if.body, if_scope);

    MIRBlock *terminator_block =
        mirgen->module.getBlock(mirgen->builder.block.get());
    if (!terminator_block->hasTerminator(&mirgen->module)) {
      mirgen->builder.buildBr(merge_block);
    }

    // Else
    if (else_block.isSome()) {
      mirgen->builder.block = else_block;

      Symbol *else_scope = scope->findSymbolByNode(node->_if._else);
      if (else_scope == nullptr) {
        else_scope = scope;
      }
      gen(mirgen, node->_if._else, else_scope);

      terminator_block = mirgen->module.getBlock(mirgen->builder.block.get());
      if (!terminator_block->hasTerminator(&mirgen->module)) {
        mirgen->builder.buildBr(merge_block);
      }
    }

    // Merge
    mirgen->builder.block.setSome(merge_block);
    break;
  }
  case NodeKind::For: {
    Symbol *for_scope = scope->findSymbolByNode(node);
    MIRBlock *entry_block =
        mirgen->module.getBlock(mirgen->builder.block.get());
    MIRValueId parent_define = entry_block->parent;

    // Blocks
    MIRBlockId condition_block =
        mirgen->builder.appendBlock(parent_define, str("for_condition"));
    MIRBlockId do_block =
        mirgen->builder.appendBlock(parent_define, str("for_do"));
    MIRBlockId merge_block =
        mirgen->builder.appendBlock(parent_define, str("for_merge"));

    mirgen->builder.buildBr(condition_block);

    // Conditional
    mirgen->builder.block.setSome(condition_block);
    MIRValueId condition = gen(mirgen, node->_for.conditional, for_scope);

    mirgen->builder.buildCondBr(condition, do_block, merge_block);

    // Do
    mirgen->builder.block.setSome(do_block);
    gen(mirgen, node->_for.body, for_scope);

    MIRBlock *terminator_block =
        mirgen->module.getBlock(mirgen->builder.block.get());
    if (!terminator_block->hasTerminator(&mirgen->module)) {
      mirgen->builder.buildBr(condition_block);
    }

    // Merge
    mirgen->builder.block.setSome(merge_block);
    break;
  }
  case NodeKind::Switch: {
    MIRBlock *entry_block =
        mirgen->module.getBlock(mirgen->builder.block.get());
    MIRValueId parent_define = entry_block->parent;
    MIRBlockId merge_block =
        mirgen->builder.appendBlock(parent_define, str("switch_merge"));

    // Cases
    MIRValueId value = gen(mirgen, node->_switch.conditional, scope);
    MIRValueId _switch = mirgen->builder.buildSwitch(
        value, merge_block, node->_switch.cases.length);
    mirgen->builder.setSourceLocation(_switch, node->location);

    // Cases
    for (size_t i = 0; i < node->_switch.cases.length; i++) {
      Node *_case = node->_switch.cases.data.ptr[i];
      Symbol *case_scope = scope->findSymbolByNode(_case);

      // Body
      MIRBlockId case_block =
          mirgen->builder.appendBlock(parent_define, str("switch_case"));
      mirgen->builder.block.setSome(case_block);
      gen(mirgen, _case->_case.body, case_scope);

      MIRBlock *terminator_block =
          mirgen->module.getBlock(mirgen->builder.block.get());
      if (!terminator_block->hasTerminator(&mirgen->module)) {
        mirgen->builder.buildBr(merge_block);
      }

      // Add
      MIRValueId constant = gen(mirgen, _case->_case.constant, scope);
      mirgen->builder.addCase(_switch, constant, case_block);
    }

    // Merge
    mirgen->builder.block.setSome(merge_block);
    break;
  }
  case NodeKind::Comptime: {
    return genComptime(mirgen, node->child, scope);
  }
  }

  return {0xFFFFFFFF, 0};
}

void genDeclaration(MIRGen *mirgen, Node *node, Symbol *scope) {
  switch (node->kind) {
  case NodeKind::Compound: {
    for (size_t i = 0; i < node->children.length; i++) {
      genDeclaration(mirgen, node->children.getUnchecked(i), scope);
    }
    break;
  }
  case NodeKind::Field: {
    MIRValueId field;
    if (node->field.initial != nullptr &&
        node->field.initial->kind == NodeKind::Function) {
      field =
          mirgen->builder.buildFunction({.ptr = nullptr}, {}, node->field.name);
      mirgen->node_to_value.insert(node->field.initial, field);
    } else {
      field = mirgen->builder.buildGlobalVariable({}, {}, node->field.name);
    }

    mirgen->node_to_value.insert(node, field);
    break;
  }
  }
}

void MIRGen::generate() {
  this->node_to_value.init(this->allocator, 32);
  this->symbol_to_scope.init(this->allocator, 32);
  this->module.init(this->allocator);
  this->module.ctx = this->ctx;
  this->builder.module = &this->module;
  this->builder.scope.setSome(this->module.definitions);
  this->builder.block.setNone();

  this->symbol_to_scope.insert(this->symbol, this->module.definitions);
  genDeclaration(this, this->ast, this->symbol);
  gen(this, this->ast, this->symbol);
}

void MIRGen::deinit() {
  this->node_to_value.deinit();
  this->module.deinit();
}
