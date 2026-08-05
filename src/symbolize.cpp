#include "allocator.hpp"
#include "ast.hpp"
#include "symbol.hpp"
#include <cstddef>

void symbolize(Allocator *allocator, Node *node, Symbol *parent) {
  if (node == nullptr) {
    return;
  }

  switch (node->kind) {
  case NodeKind::Compound: {
    for (size_t i = 0; i < node->children.length; i++) {
      symbolize(allocator, node->children.data.ptr[i], parent);
    }
    break;
  }
  case NodeKind::Name:
  case NodeKind::RawString:
  case NodeKind::Value: {
    break;
  }
  case NodeKind::Field: {
    Symbol *symbol = (Symbol *)allocator->alloc(sizeof(Symbol));
    symbol->init(allocator, false, parent);
    symbol->node = node;
    symbol->name = &node->field.name;

    symbolize(allocator, node->field.type, symbol);
    symbolize(allocator, node->field.initial, symbol);
    break;
  }
  case NodeKind::Function: {
    Symbol *symbol = (Symbol *)allocator->alloc(sizeof(Symbol));
    symbol->init(allocator, true, parent);
    symbol->node = node;

    for (size_t i = 0; i < node->function.parameters.length; i++) {
      symbolize(allocator, node->function.parameters.data.ptr[i], symbol);
    }

    symbolize(allocator, node->function.return_type, symbol);
    symbolize(allocator, node->function.body, symbol);
    break;
  }
  case NodeKind::Struct: {
    Symbol *symbol = (Symbol *)allocator->alloc(sizeof(Symbol));
    symbol->init(allocator, false, parent);
    symbol->node = node;

    for (size_t i = 0; i < node->_struct.fields.length; i++) {
      symbolize(allocator, node->_struct.fields.data.ptr[i], symbol);
    }

    for (size_t i = 0; i < node->_struct.body.length; i++) {
      symbolize(allocator, node->_struct.body.data.ptr[i], symbol);
    }
    break;
  }
  case NodeKind::Enum: {
    Symbol *symbol = (Symbol *)allocator->alloc(sizeof(Symbol));
    symbol->init(allocator, false, parent);
    symbol->node = node;

    symbolize(allocator, node->_enum.repr_type, symbol);
    for (size_t i = 0; i < node->_enum.members.length; i++) {
      symbolize(allocator, node->_enum.members.data.ptr[i], symbol);
    }

    for (size_t i = 0; i < node->_enum.body.length; i++) {
      symbolize(allocator, node->_enum.body.data.ptr[i], symbol);
    }
    break;
  }
  case NodeKind::Union: {
    Symbol *symbol = (Symbol *)allocator->alloc(sizeof(Symbol));
    symbol->init(allocator, false, parent);
    symbol->node = node;

    symbolize(allocator, node->_union.repr_type, symbol);
    for (size_t i = 0; i < node->_union.variants.length; i++) {
      symbolize(allocator, node->_union.variants.data.ptr[i], symbol);
    }

    for (size_t i = 0; i < node->_union.body.length; i++) {
      symbolize(allocator, node->_union.body.data.ptr[i], symbol);
    }
    break;
  }
  case NodeKind::Namespace: {
    Symbol *symbol = (Symbol *)allocator->alloc(sizeof(Symbol));
    symbol->init(allocator, false, parent);
    symbol->node = node;

    for (size_t i = 0; i < node->children.length; i++) {
      symbolize(allocator, node->children.data.ptr[i], symbol);
    }
    break;
  }
  case NodeKind::Member: {
    Symbol *symbol = (Symbol *)allocator->alloc(sizeof(Symbol));
    symbol->init(allocator, false, parent);
    symbol->node = node;
    symbol->name = &node->member.name;

    symbolize(allocator, node->member.value, symbol);
    break;
  }
  case NodeKind::Import: {
    break;
  }
  case NodeKind::Const: {
    symbolize(allocator, node->child, parent);
    break;
  }
  case NodeKind::Slice: {
    symbolize(allocator, node->slice.length, parent);
    symbolize(allocator, node->slice.type, parent);
    break;
  }
  case NodeKind::Assignment: {
    symbolize(allocator, node->_operator.lhs, parent);
    symbolize(allocator, node->_operator.rhs, parent);
    break;
  }
  case NodeKind::UnaryOperator: {
    symbolize(allocator, node->unary_operator.child, parent);
    break;
  }
  case NodeKind::Operator: {
    symbolize(allocator, node->_operator.lhs, parent);
    symbolize(allocator, node->_operator.rhs, parent);
    break;
  }
  case NodeKind::Range: {
    symbolize(allocator, node->range.min, parent);
    symbolize(allocator, node->range.max, parent);
    break;
  }
  case NodeKind::Call: {
    symbolize(allocator, node->call.callee, parent);
    for (size_t i = 0; i < node->call.arguments.length; i++) {
      symbolize(allocator, node->call.arguments.data.ptr[i], parent);
    }
    break;
  }
  case NodeKind::Index: {
    symbolize(allocator, node->index.slice, parent);
    symbolize(allocator, node->index.index, parent);
    break;
  }
  case NodeKind::Initializer: {
    symbolize(allocator, node->initializer.record, parent);
    for (size_t i = 0; i < node->initializer.setters.length; i++) {
      symbolize(allocator, node->initializer.setters.data.ptr[i], parent);
    }
    break;
  }
  case NodeKind::Return: {
    symbolize(allocator, node->child, parent);
    break;
  }
  case NodeKind::If: {
    Symbol *symbol = (Symbol *)allocator->alloc(sizeof(Symbol));
    symbol->init(allocator, true, parent);
    symbol->node = node;

    symbolize(allocator, node->_if.conditional, symbol);
    symbolize(allocator, node->_if.body, symbol);

    if (node->_if._else != nullptr) {
      Symbol *else_symbol = parent;
      if (node->_if._else->kind == NodeKind::Compound) {
        else_symbol = (Symbol *)allocator->alloc(sizeof(Symbol));
        else_symbol->init(allocator, true, parent);
        else_symbol->node = node->_if._else;
      }

      symbolize(allocator, node->_if._else, else_symbol);
    }
    break;
  }
  case NodeKind::For: {
    Symbol *symbol = (Symbol *)allocator->alloc(sizeof(Symbol));
    symbol->init(allocator, true, parent);
    symbol->node = node;

    symbolize(allocator, node->_for.conditional, symbol);
    symbolize(allocator, node->_for.body, symbol);
    break;
  }
  case NodeKind::In: {
    Symbol *symbol = (Symbol *)allocator->alloc(sizeof(Symbol));
    symbol->init(allocator, false, parent);
    symbol->node = node;
    symbol->name = &node->in.name;

    symbolize(allocator, node->in.range, parent);
    break;
  }
  case NodeKind::Switch: {
    for (size_t i = 0; i < node->_switch.cases.length; i++) {
      symbolize(allocator, node->_switch.cases.data.ptr[i], parent);
    }
    break;
  }
  case NodeKind::Case: {
    Symbol *symbol = (Symbol *)allocator->alloc(sizeof(Symbol));
    symbol->init(allocator, true, parent);
    symbol->node = node;

    symbolize(allocator, node->_case.body, symbol);
    break;
  }
  case NodeKind::Break:
  case NodeKind::Continue: {
    break;
  }
  case NodeKind::Defer:
  case NodeKind::Comptime: {
    symbolize(allocator, node->child, parent);
    break;
  }
  case NodeKind::Assembly:
  case NodeKind::Attribute:
  case NodeKind::CommentGroup:
  case NodeKind::Dead: {
    break;
  }
  }
}
