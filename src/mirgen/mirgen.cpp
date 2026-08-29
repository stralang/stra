#include "mirgen.hpp"
#include "../print.hpp"
#include "define.hpp"
#include "literal.hpp"
#include "mir.hpp"
#include <iostream>

MIRValue *addr(MIRGen *mirgen, Node *node, Symbol *scope) {
  switch (node->kind) {
  case NodeKind::Name: {
    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    MIRValue **value = mirgen->node_to_value.get(symbol->node);
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
  }

  return nullptr;
}

MIRValue *gen(MIRGen *mirgen, Node *node, Symbol *scope) {
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
    MIRValue *builtin = genBuiltin(mirgen, node->text);
    if (builtin != nullptr) {
      return builtin;
    }

    Symbol *symbol = scope->findSymbol(&node->text, &node->location);
    MIRValue **value = mirgen->node_to_value.get(symbol->node);
    if (value == nullptr) {
      std::cerr << node->location << " Couldn't find value `" << node->text
                << "`. Aborting...\n";
      std::abort();
    }

    MIRValue *out = mirgen->builder.buildLoad(*value, {.ptr = nullptr});
    out->source_location = node->location;
    return out;
  }
  case NodeKind::Value: {
    return valueToMIR(mirgen, &node->value);
  }
  case NodeKind::Field: {
    // Get Symbol
    Symbol *field_symbol = scope->findSymbolByNode(node);

    // TODO: Add attributes

    // Generate field
    MIRValue *field = nullptr;
    if (scope->location_aware) {
      MIRValue *initial = nullptr;
      MIRValue *type = nullptr;
      if (node->field.initial != nullptr) {
        initial = gen(mirgen, node->field.initial, field_symbol);
      }
      if (node->field.type != nullptr) {
        type = gen(mirgen, node->field.type, field_symbol);
      } else {
        type = mirgen->builder.buildTypeOf(initial, {.ptr = nullptr});
      }

      field = mirgen->builder.buildAlloca(type, node->field.name);
      mirgen->node_to_value.insert(node, field);

      if (initial != nullptr) {
        mirgen->builder.buildStore(initial, field);
      }
    } else if (node->field.initial != nullptr &&
               node->field.initial->kind == NodeKind::Function) {
      field = gen(mirgen, node->field.initial, field_symbol);
    } else {
      field = *mirgen->node_to_value.get(node); // Pre-generated

      if (node->field.type != nullptr) {
        field->global_variable.type =
            gen(mirgen, node->field.type, field_symbol);
      }
      if (node->field.initial != nullptr) {
        field->global_variable.constant =
            gen(mirgen, node->field.initial, field_symbol);
      }
      field->global_variable.undefined = node->field.undefined;
    }

    field->source_location = node->location;
    return field;
  }
  case NodeKind::Function: {
    Symbol *fn_symbol = scope->findSymbolByNode(node);

    // Parameter Types
    Slice<MIRValue *> parameters = {
        .ptr = (MIRValue **)mirgen->allocator->alloc(
            sizeof(MIRValue *) * node->function.parameters.length),
        .len = node->function.parameters.length,
    };

    for (size_t i = 0; i < node->function.parameters.length; i++) {
      Node *arg = node->function.parameters.getUnchecked(i);
      parameters.ptr[i] = gen(mirgen, arg->field.type, fn_symbol);
    }

    // Return Type
    MIRValue *return_type = nullptr;
    if (node->function.return_type == nullptr) {
      MIRValue void_ty = {.kind = MIRValueKind::Literal};
      void_ty.literal.lit_type =
          mirgen->ctx->type_cache->get({.kind = TypeKind::TypeId});
      void_ty.literal._typeid =
          mirgen->ctx->type_cache->get({.kind = TypeKind::Void});
      void_ty.result_type = void_ty.literal.lit_type;

      return_type = mirgen->ctx->make(void_ty);
    } else {
      return_type = gen(mirgen, node->function.return_type, fn_symbol);
    }

    // Build Function
    MIRValue *value = nullptr;
    MIRValue **cache = mirgen->node_to_value.get(node);
    if (cache != nullptr) {
      value = *cache; // Pre-generated
      value->function.parameter_types = parameters;
      value->function.return_type = return_type;
    } else {
      value = mirgen->builder.buildFunction(parameters, return_type, str(""));
    }
    value->source_location = node->location;

    // Body
    value->function.undefined = node->function.undefined;
    if (node->function.body != nullptr) {
      MIRScope *globals =
          (MIRScope *)mirgen->module.arena.alloc(sizeof(MIRScope));
      globals->owner = value;
      globals->list.init(mirgen->builder.module->arena.allocator, 32);
      value->function.globals = globals;

      value->function.blocks.init(mirgen->allocator, 32);
      MIRBlock *entry_block = mirgen->builder.appendBlock(value, str("entry"));

      MIRBlock *prev_block = mirgen->builder.block;
      MIRScope *prev_scope = mirgen->builder.scope;
      mirgen->builder.block = entry_block;
      mirgen->builder.scope = globals;

      // Parameters
      for (size_t i = 0; i < node->function.parameters.length; i++) {
        Node *arg = node->function.parameters.getUnchecked(i);
        MIRValue *mir_arg = mirgen->builder.buildArg(
            value->function.parameter_types.ptr[i], arg->field.name);
        mir_arg->source_location = arg->location;
        mirgen->node_to_value.insert(arg, mir_arg);
      }

      // Body
      gen(mirgen, node->function.body, fn_symbol);

      // End
      mirgen->builder.block = prev_block;
      mirgen->builder.scope = prev_scope;
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
    MIRValue *callee = addr(mirgen, node->call.callee, scope);
    Slice<MIRValue *> arguments = {
        .ptr = (MIRValue **)mirgen->allocator->alloc(
            sizeof(MIRValue *) * node->call.arguments.length),
        .len = node->call.arguments.length,
    };

    // Receiver
    MIRValue *receiver = nullptr;
    if (node->call.callee->_operator.opcode == Operator::MemberAccess) {
      receiver = addr(mirgen, node->call.callee->_operator.lhs, scope);
    }

    // Arguments
    for (size_t i = 0; i < arguments.len; i++) {
      arguments.ptr[i] =
          gen(mirgen, node->call.arguments.getUnchecked(i), scope);
    }

    MIRValue *out = mirgen->builder.buildCall(callee, arguments, receiver,
                                              {.ptr = nullptr});
    out->source_location = node->location;
    return out;
  }
  case NodeKind::Return: {
    MIRValue *ret_value = nullptr;
    if (node->child != nullptr) {
      ret_value = gen(mirgen, node->child, scope);
    }

    MIRValue *out = mirgen->builder.buildReturn(ret_value);
    out->source_location = node->location;
    return out;
  }
  case NodeKind::If: {
    Symbol *if_scope = scope->findSymbolByNode(node);
    MIRValue *parent_define = mirgen->builder.block->parent;

    // Blocks
    MIRBlock *then_block =
        mirgen->builder.appendBlock(parent_define, str("if_then"));
    MIRBlock *else_block = nullptr;
    if (node->_if._else != nullptr) {
      else_block = mirgen->builder.appendBlock(parent_define, str("if_else"));
    }

    MIRBlock *merge_block =
        mirgen->builder.appendBlock(parent_define, str("if_merge"));

    // Conditional
    MIRValue *condition = gen(mirgen, node->_if.conditional, scope);

    if (else_block != nullptr) {
      mirgen->builder.buildCondBr(condition, then_block, else_block);
    } else {
      mirgen->builder.buildCondBr(condition, then_block, merge_block);
    }

    // Body
    mirgen->builder.block = then_block;
    gen(mirgen, node->_if.body, if_scope);

    if (!mirgen->builder.block->hasTerminator()) {
      mirgen->builder.buildBr(merge_block);
    }

    // Else
    if (else_block != nullptr) {
      mirgen->builder.block = else_block;

      Symbol *else_scope = scope->findSymbolByNode(node->_if._else);
      if (else_scope == nullptr) {
        else_scope = scope;
      }
      gen(mirgen, node->_if._else, else_scope);

      if (!mirgen->builder.block->hasTerminator()) {
        mirgen->builder.buildBr(merge_block);
      }
    }

    // Merge
    mirgen->builder.block = merge_block;
    break;
  }
  case NodeKind::For: {
    Symbol *for_scope = scope->findSymbolByNode(node);
    MIRValue *parent_define = mirgen->builder.block->parent;

    // Blocks
    MIRBlock *condition_block =
        mirgen->builder.appendBlock(parent_define, str("for_condition"));
    MIRBlock *do_block =
        mirgen->builder.appendBlock(parent_define, str("for_do"));
    MIRBlock *merge_block =
        mirgen->builder.appendBlock(parent_define, str("for_merge"));

    mirgen->builder.buildBr(condition_block);

    // Conditional
    mirgen->builder.block = condition_block;
    MIRValue *condition = gen(mirgen, node->_for.conditional, for_scope);

    mirgen->builder.buildCondBr(condition, do_block, merge_block);

    // Do
    mirgen->builder.block = do_block;
    gen(mirgen, node->_for.body, for_scope);

    if (!mirgen->builder.block->hasTerminator()) {
      mirgen->builder.buildBr(condition_block);
    }

    // Merge
    mirgen->builder.block = merge_block;
    break;
  }
  case NodeKind::Switch: {
    MIRValue *parent_define = mirgen->builder.block->parent;
    MIRBlock *merge_block =
        mirgen->builder.appendBlock(parent_define, str("switch_merge"));

    // Cases
    MIRValue *value = gen(mirgen, node->_switch.conditional, scope);
    MIRValue *_switch = mirgen->builder.buildSwitch(value, merge_block,
                                                    node->_switch.cases.length);
    _switch->source_location = node->location;

    // Cases
    for (size_t i = 0; i < node->_switch.cases.length; i++) {
      Node *_case = node->_switch.cases.data.ptr[i];
      Symbol *case_scope = scope->findSymbolByNode(_case);

      // Body
      MIRBlock *case_block =
          mirgen->builder.appendBlock(parent_define, str("switch_case"));
      mirgen->builder.block = case_block;
      gen(mirgen, _case->_case.body, case_scope);

      if (!mirgen->builder.block->hasTerminator()) {
        mirgen->builder.buildBr(merge_block);
      }

      // Add
      MIRValue *constant = gen(mirgen, _case->_case.constant, scope);
      mirgen->builder.addCase(_switch, constant, case_block);
    }

    // Merge
    mirgen->builder.block = merge_block;
    break;
  }
  case NodeKind::Comptime: {
    MIRValue *value = mirgen->builder.buildComptime({.ptr = nullptr});
    value->comptime.blocks.init(mirgen->allocator, 32);

    MIRBlock *prev_block = mirgen->builder.block;
    mirgen->builder.block = mirgen->builder.appendBlock(value, str("entry"));

    MIRValue *output = gen(mirgen, node->child, scope);
    if (!mirgen->builder.block->hasTerminator()) {
      mirgen->builder.buildReturn(output);
    }

    mirgen->builder.block = prev_block;
    return value;
  }
  }

  return nullptr;
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
    MIRValue *field = nullptr;
    if (node->field.initial != nullptr &&
        node->field.initial->kind == NodeKind::Function) {
      field = mirgen->builder.buildFunction({.ptr = nullptr}, nullptr,
                                            node->field.name);
      mirgen->node_to_value.insert(node->field.initial, field);
    } else {
      field = mirgen->builder.buildGlobalVariable(nullptr, nullptr,
                                                  node->field.name);
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
  this->builder.scope = this->module.definitions;
  this->builder.block = nullptr;

  this->symbol_to_scope.insert(this->symbol, this->module.definitions);
  genDeclaration(this, this->ast, this->symbol);
  gen(this, this->ast, this->symbol);
}

void MIRGen::deinit() {
  this->node_to_value.deinit();
  this->module.deinit();
}
