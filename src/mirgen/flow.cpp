#include "define.hpp"

void genIf(MIRGen *mirgen, Node *node, Symbol *scope) {
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

  // Then
  {
    // Set Defer
    size_t old_defer_len = mirgen->defer_stack_len;
    size_t old_defer_boundary = mirgen->defer_local_boundary;

    // Body
    mirgen->builder.block = then_block;
    gen(mirgen, node->_if.body, if_scope);

    if (!mirgen->builder.block->hasTerminator()) {
      injectDefer(mirgen, if_scope, false);
      mirgen->builder.buildBr(merge_block);
    }

    // Reset Defer
    mirgen->defer_stack_len = old_defer_len;
    mirgen->defer_local_boundary = old_defer_boundary;
  }

  // Else
  if (else_block != nullptr) {
    // Set Defer
    size_t old_defer_len = mirgen->defer_stack_len;
    size_t old_defer_boundary = mirgen->defer_local_boundary;

    // Body
    mirgen->builder.block = else_block;

    Symbol *else_scope = scope->findSymbolByNode(node->_if._else);
    if (else_scope == nullptr) {
      else_scope = scope;
    }
    gen(mirgen, node->_if._else, else_scope);

    if (!mirgen->builder.block->hasTerminator()) {
      injectDefer(mirgen, else_scope, false);
      mirgen->builder.buildBr(merge_block);
    }

    // Reset Defer
    mirgen->defer_stack_len = old_defer_len;
    mirgen->defer_local_boundary = old_defer_boundary;
  }

  // Merge
  mirgen->builder.block = merge_block;
}

void genLoop(MIRGen *mirgen, Node *node, Symbol *scope) {
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

  {
    // Set Defer
    size_t old_defer_len = mirgen->defer_stack_len;
    size_t old_defer_boundary = mirgen->defer_local_boundary;

    // Do
    mirgen->builder.block = do_block;
    gen(mirgen, node->_for.body, for_scope);

    if (!mirgen->builder.block->hasTerminator()) {
      injectDefer(mirgen, for_scope, false);
      mirgen->builder.buildBr(condition_block);
    }

    // Reset Defer
    mirgen->defer_stack_len = old_defer_len;
    mirgen->defer_local_boundary = old_defer_boundary;
  }

  // Merge
  mirgen->builder.block = merge_block;
}

void genSwitch(MIRGen *mirgen, Node *node, Symbol *scope) {
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

    MIRBlock *case_block = nullptr;
    {
      // Set Defer
      size_t old_defer_len = mirgen->defer_stack_len;
      size_t old_defer_boundary = mirgen->defer_local_boundary;

      // Body
      case_block =
          mirgen->builder.appendBlock(parent_define, str("switch_case"));
      mirgen->builder.block = case_block;
      gen(mirgen, _case->_case.body, case_scope);

      if (!mirgen->builder.block->hasTerminator()) {
        injectDefer(mirgen, case_scope, false);
        mirgen->builder.buildBr(merge_block);
      }

      // Reset Defer
      mirgen->defer_stack_len = old_defer_len;
      mirgen->defer_local_boundary = old_defer_boundary;
    }

    // Add
    MIRValue *constant = gen(mirgen, _case->_case.constant, scope);
    mirgen->builder.addCase(_switch, constant, case_block);
  }

  // Merge
  mirgen->builder.block = merge_block;
}
