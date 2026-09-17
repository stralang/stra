#include "define.hpp"
#include "uir/literal.hpp"
#include "uir/uir.hpp"

void genIf(UIRGen *uirgen, Node *node, Symbol *scope) {
  Symbol *if_scope = scope->findSymbolByNode(node);
  UIRValue *parent_define = uirgen->builder.block->parent;

  // Blocks
  UIRBlock *then_block =
      uirgen->builder.appendBlock(parent_define, str("if_then"));
  UIRBlock *else_block = nullptr;
  if (node->_if._else != nullptr) {
    else_block = uirgen->builder.appendBlock(parent_define, str("if_else"));
  }

  UIRBlock *merge_block =
      uirgen->builder.appendBlock(parent_define, str("if_merge"));

  // Conditional
  UIRValue *condition = gen(uirgen, node->_if.conditional, scope);

  if (else_block != nullptr) {
    uirgen->builder.buildCondBr(condition, then_block, else_block);
  } else {
    uirgen->builder.buildCondBr(condition, then_block, merge_block);
  }

  // Then
  {
    // Set Defer
    size_t old_defer_len = uirgen->defer_stack_len;
    size_t old_defer_boundary = uirgen->defer_local_boundary;

    // Body
    uirgen->builder.block = then_block;
    gen(uirgen, node->_if.body, if_scope);

    if (!uirgen->builder.block->hasTerminator()) {
      injectDefer(uirgen, if_scope, false);
      uirgen->builder.buildBr(merge_block);
    }

    // Reset Defer
    uirgen->defer_stack_len = old_defer_len;
    uirgen->defer_local_boundary = old_defer_boundary;
  }

  // Else
  if (else_block != nullptr) {
    // Set Defer
    size_t old_defer_len = uirgen->defer_stack_len;
    size_t old_defer_boundary = uirgen->defer_local_boundary;

    // Body
    uirgen->builder.block = else_block;

    Symbol *else_scope = scope->findSymbolByNode(node->_if._else);
    if (else_scope == nullptr) {
      else_scope = scope;
    }
    gen(uirgen, node->_if._else, else_scope);

    if (!uirgen->builder.block->hasTerminator()) {
      injectDefer(uirgen, else_scope, false);
      uirgen->builder.buildBr(merge_block);
    }

    // Reset Defer
    uirgen->defer_stack_len = old_defer_len;
    uirgen->defer_local_boundary = old_defer_boundary;
  }

  // Merge
  uirgen->builder.block = merge_block;
}

void genLoop(UIRGen *uirgen, Node *node, Symbol *scope) {
  Symbol *for_scope = scope->findSymbolByNode(node);
  UIRValue *parent_define = uirgen->builder.block->parent;

  // `for-in` Init
  bool range_loop = node->_for.conditional->kind == NodeKind::In;
  if (range_loop) {
    UIRBlock *init_block =
        uirgen->builder.appendBlock(parent_define, str("for_init"));
    uirgen->builder.buildBr(init_block);
    uirgen->builder.block = init_block;

    UIRValue *initial =
        gen(uirgen, node->_for.conditional->in.range->range.min, for_scope);
    UIRValue *type;
    if (initial->kind == UIRValueKind::Literal) {
      UIRLiteral literal = {
          .lit_type = uirgen->ctx->type_cache->get({.kind = TypeKind::TypeId}),
          .kind = UIRLiteralKind::Typed,
          ._typeid = uirgen->ctx->type_cache->get({
              .kind = TypeKind::Integer,
              .integer = {false, false, -1},
              .is_constant = true,
          })};
      type = uirgen->builder.buildLiteral(literal);
    } else {
      type = uirgen->builder.buildTypeOf(initial);
    }

    UIRValue *field = uirgen->builder.buildLocalVariable(
        type, node->_for.conditional->in.name);
    field->source_location = node->_for.conditional->location;

    uirgen->builder.buildStore(initial, field);
    uirgen->node_to_value.insert(node->_for.conditional, field);
  }

  // Blocks
  UIRBlock *condition_block =
      uirgen->builder.appendBlock(parent_define, str("for_condition"));
  UIRBlock *do_block =
      uirgen->builder.appendBlock(parent_define, str("for_do"));
  UIRBlock *merge_block =
      uirgen->builder.appendBlock(parent_define, str("for_merge"));
  UIRBlock *continue_block = condition_block;

  uirgen->builder.buildBr(condition_block);

  // Conditional
  uirgen->builder.block = condition_block;
  UIRValue *condition;
  if (range_loop) {
    // `for-in` Conditional
    UIRValue *lhs_ptr = *uirgen->node_to_value.get(node->_for.conditional);
    UIRValue *lhs = uirgen->builder.buildLoad(lhs_ptr);
    UIRValue *rhs =
        gen(uirgen, node->_for.conditional->in.range->range.max, for_scope);

    UIROpcode opcode =
        node->_for.conditional->in.range->range.mode == NodeRange::EqualTo
            ? UIROpcode::LessThenOrEqualTo
            : UIROpcode::LessThen;

    condition = uirgen->builder.buildBinOp(lhs, rhs, opcode);
  } else {
    condition = gen(uirgen, node->_for.conditional, for_scope);
  }

  uirgen->builder.buildCondBr(condition, do_block, merge_block);

  // `for-in` Increment
  if (range_loop) {
    UIRBlock *increment_block =
        uirgen->builder.appendBlock(parent_define, str("for_increment"));
    continue_block = increment_block;
    uirgen->builder.block = increment_block;

    UIRValue *ptr = *uirgen->node_to_value.get(node->_for.conditional);
    UIRValue *idx = uirgen->builder.buildLoad(ptr);
    UIRValue *one = uirgen->builder.buildLiteral(
        {.lit_type = ptr->local_variable.type->literal._typeid,
         .kind = UIRLiteralKind::Typed,
         ._int = 1});
    UIRValue *result = uirgen->builder.buildBinOp(idx, one, UIROpcode::Add);
    uirgen->builder.buildStore(result, ptr);

    uirgen->builder.buildBr(condition_block);
  }

  // Body
  {
    // Set Defer
    size_t old_defer_len = uirgen->defer_stack_len;
    size_t old_defer_boundary = uirgen->defer_local_boundary;

    // Do
    uirgen->builder.block = do_block;
    gen(uirgen, node->_for.body, for_scope);

    if (!uirgen->builder.block->hasTerminator()) {
      injectDefer(uirgen, for_scope, false);
      uirgen->builder.buildBr(continue_block);
    }

    // Reset Defer
    uirgen->defer_stack_len = old_defer_len;
    uirgen->defer_local_boundary = old_defer_boundary;
  }

  // Merge
  uirgen->builder.block = merge_block;
}

void genSwitch(UIRGen *uirgen, Node *node, Symbol *scope) {
  UIRValue *parent_define = uirgen->builder.block->parent;
  UIRBlock *merge_block =
      uirgen->builder.appendBlock(parent_define, str("switch_merge"));

  // Cases
  UIRValue *value = gen(uirgen, node->_switch.conditional, scope);
  UIRValue *_switch = uirgen->builder.buildSwitch(value, merge_block,
                                                  node->_switch.cases.length);
  _switch->source_location = node->location;

  // Cases
  for (size_t i = 0; i < node->_switch.cases.length; i++) {
    Node *_case = node->_switch.cases.data.ptr[i];
    Symbol *case_scope = scope->findSymbolByNode(_case);

    UIRBlock *case_block = nullptr;
    {
      // Set Defer
      size_t old_defer_len = uirgen->defer_stack_len;
      size_t old_defer_boundary = uirgen->defer_local_boundary;

      // Body
      case_block =
          uirgen->builder.appendBlock(parent_define, str("switch_case"));
      uirgen->builder.block = case_block;
      gen(uirgen, _case->_case.body, case_scope);

      if (!uirgen->builder.block->hasTerminator()) {
        injectDefer(uirgen, case_scope, false);
        uirgen->builder.buildBr(merge_block);
      }

      // Reset Defer
      uirgen->defer_stack_len = old_defer_len;
      uirgen->defer_local_boundary = old_defer_boundary;
    }

    // Add
    UIRValue *constant = gen(uirgen, _case->_case.constant, scope);
    uirgen->builder.addCase(_switch, constant, case_block);
  }

  // Merge
  uirgen->builder.block = merge_block;
}
