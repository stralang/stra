#include "define.hpp"
#include "literal.hpp"
#include "mir.hpp"

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

  // `for-in` Init
  bool range_loop = node->_for.conditional->kind == NodeKind::In;
  if (range_loop) {
    MIRBlock *init_block =
        mirgen->builder.appendBlock(parent_define, str("for_init"));
    mirgen->builder.buildBr(init_block);
    mirgen->builder.block = init_block;

    MIRValue *initial =
        gen(mirgen, node->_for.conditional->in.range->range.min, for_scope);
    MIRValue *type;
    if (initial->kind == MIRValueKind::Literal) {
      type = mirgen->ctx->makeLiteral({
          .lit_type = mirgen->ctx->type_cache->get({.kind = TypeKind::TypeId}),
          .kind = MIRLiteralKind::Typed,
          ._typeid = mirgen->ctx->type_cache->get({
              .kind = TypeKind::Integer,
              .integer = {false, false, -1},
              .is_constant = true,
          }),
      });
    } else {
      type = mirgen->builder.buildTypeOf(initial);
    }

    MIRValue *field = mirgen->builder.buildLocalVariable(
        type, node->_for.conditional->in.name);
    field->source_location = node->_for.conditional->location;

    mirgen->builder.buildStore(initial, field);
    mirgen->node_to_value.insert(node->_for.conditional, field);
  }

  // Blocks
  MIRBlock *condition_block =
      mirgen->builder.appendBlock(parent_define, str("for_condition"));
  MIRBlock *do_block =
      mirgen->builder.appendBlock(parent_define, str("for_do"));
  MIRBlock *merge_block =
      mirgen->builder.appendBlock(parent_define, str("for_merge"));
  MIRBlock *continue_block = condition_block;

  mirgen->builder.buildBr(condition_block);

  // Conditional
  mirgen->builder.block = condition_block;
  MIRValue *condition;
  if (range_loop) {
    // `for-in` Conditional
    MIRValue *lhs_ptr = *mirgen->node_to_value.get(node->_for.conditional);
    MIRValue *lhs = mirgen->builder.buildLoad(lhs_ptr);
    MIRValue *rhs =
        gen(mirgen, node->_for.conditional->in.range->range.max, for_scope);

    MIROpcode opcode =
        node->_for.conditional->in.range->range.mode == NodeRange::EqualTo
            ? MIROpcode::LessThenOrEqualTo
            : MIROpcode::LessThen;

    condition = mirgen->builder.buildBinOp(lhs, rhs, opcode);
  } else {
    condition = gen(mirgen, node->_for.conditional, for_scope);
  }

  mirgen->builder.buildCondBr(condition, do_block, merge_block);

  // `for-in` Increment
  if (range_loop) {
    MIRBlock *increment_block =
        mirgen->builder.appendBlock(parent_define, str("for_increment"));
    continue_block = increment_block;
    mirgen->builder.block = increment_block;

    MIRValue *ptr = *mirgen->node_to_value.get(node->_for.conditional);
    MIRValue *idx = mirgen->builder.buildLoad(ptr);
    MIRValue *one = mirgen->ctx->makeLiteral(
        {.lit_type = ptr->local_variable.type->literal._typeid,
         .kind = MIRLiteralKind::Typed,
         ._int = 1});
    MIRValue *result = mirgen->builder.buildBinOp(idx, one, MIROpcode::Add);
    mirgen->builder.buildStore(result, ptr);

    mirgen->builder.buildBr(condition_block);
  }

  // Body
  {
    // Set Defer
    size_t old_defer_len = mirgen->defer_stack_len;
    size_t old_defer_boundary = mirgen->defer_local_boundary;

    // Do
    mirgen->builder.block = do_block;
    gen(mirgen, node->_for.body, for_scope);

    if (!mirgen->builder.block->hasTerminator()) {
      injectDefer(mirgen, for_scope, false);
      mirgen->builder.buildBr(continue_block);
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
