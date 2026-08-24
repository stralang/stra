#include "define.hpp"

void genList(MIRGen *mirgen, ArrayList<Node *> *list, Symbol *scope,
             MIRScope *out) {
  out->list.init(mirgen->module.arena.allocator, list->length);

  MIRBlock *prev_block = mirgen->builder.block;
  MIRScope *prev_scope = mirgen->builder.scope;
  mirgen->builder.block = nullptr;
  mirgen->builder.scope = out;

  for (size_t i = 0; i < list->length; i++) {
    gen(mirgen, list->getUnchecked(i), scope);
  }

  mirgen->builder.scope = prev_scope;
  mirgen->builder.block = prev_block;
}

MIRValue *genStruct(MIRGen *mirgen, Node *node, Symbol *scope) {
  Symbol *struct_symbol = scope->findSymbolByNode(node);

  MIRValue *value = mirgen->ctx->make({.kind = MIRValueKind::Struct});
  value->source_location = node->location;
  value->_struct.fields =
      (MIRScope *)mirgen->module.arena.alloc(sizeof(MIRScope));
  value->_struct.definitions =
      (MIRScope *)mirgen->module.arena.alloc(sizeof(MIRScope));
  mirgen->symbol_to_scope.insert(struct_symbol, value->_struct.definitions);

  genList(mirgen, &node->_struct.fields, struct_symbol, value->_struct.fields);
  genList(mirgen, &node->_struct.body, struct_symbol,
          value->_struct.definitions);

  return value;
}

MIRValue *genEnum(MIRGen *mirgen, Node *node, Symbol *scope) {
  Symbol *enum_symbol = scope->findSymbolByNode(node);

  MIRValue *value = mirgen->ctx->make({.kind = MIRValueKind::Enum});
  value->source_location = node->location;
  value->_enum.repr_type = gen(mirgen, node->_enum.repr_type, enum_symbol);
  value->_enum.members =
      (MIRScope *)mirgen->module.arena.alloc(sizeof(MIRScope));
  value->_enum.definitions =
      (MIRScope *)mirgen->module.arena.alloc(sizeof(MIRScope));
  mirgen->symbol_to_scope.insert(enum_symbol, value->_enum.definitions);

  genList(mirgen, &node->_enum.members, enum_symbol, value->_enum.members);
  genList(mirgen, &node->_enum.body, enum_symbol, value->_enum.definitions);
  return value;
}

MIRValue *genUnion(MIRGen *mirgen, Node *node, Symbol *scope) {
  Symbol *union_symbol = scope->findSymbolByNode(node);

  MIRValue *value = mirgen->ctx->make({.kind = MIRValueKind::Union});
  value->source_location = node->location;
  value->_union.repr_type = gen(mirgen, node->_union.repr_type, union_symbol);
  value->_union.members =
      (MIRScope *)mirgen->module.arena.alloc(sizeof(MIRScope));
  value->_union.definitions =
      (MIRScope *)mirgen->module.arena.alloc(sizeof(MIRScope));
  mirgen->symbol_to_scope.insert(union_symbol, value->_union.definitions);

  genList(mirgen, &node->_union.variants, union_symbol, value->_union.members);
  genList(mirgen, &node->_union.body, union_symbol, value->_union.definitions);
  return value;
}

MIRValue *genNamespace(MIRGen *mirgen, Node *node, Symbol *scope) {
  Symbol *namespace_symbol = scope->findSymbolByNode(node);

  MIRValue *value = mirgen->ctx->make({.kind = MIRValueKind::Namespace});
  value->source_location = node->location;
  value->_namespace.definitions =
      (MIRScope *)mirgen->module.arena.alloc(sizeof(MIRScope));
  mirgen->symbol_to_scope.insert(namespace_symbol,
                                 value->_namespace.definitions);

  genList(mirgen, &node->children, namespace_symbol,
          value->_namespace.definitions);
  return value;
}
