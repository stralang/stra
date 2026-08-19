#include "define.hpp"

Slice<MIRValue *> genList(MIRGen *mirgen, ArrayList<Node *> *list,
                          Symbol *scope) {
  Slice<MIRValue *> out = {
      .ptr = (MIRValue **)mirgen->allocator->alloc(sizeof(MIRValue *) *
                                                   list->length),
      .len = list->length,
  };

  for (size_t i = 0; i < list->length; i++) {
    MIRValue *definition = gen(mirgen, list->getUnchecked(i), scope);
    out.ptr[i] = definition;
  }

  return out;
}

MIRValue *genStruct(MIRGen *mirgen, Node *node, Symbol *scope) {
  Symbol *struct_symbol = scope->findSymbolByNode(node);

  MIRValue *value = mirgen->ctx->make({.kind = MIRValueKind::Struct});
  value->_struct.fields = genList(mirgen, &node->_struct.fields, struct_symbol);
  value->_struct.definitions =
      genList(mirgen, &node->_struct.body, struct_symbol);
  return value;
}

MIRValue *genEnum(MIRGen *mirgen, Node *node, Symbol *scope) {
  Symbol *enum_symbol = scope->findSymbolByNode(node);

  MIRValue *value = mirgen->ctx->make({.kind = MIRValueKind::Enum});
  value->_enum.repr_type = gen(mirgen, node->_enum.repr_type, enum_symbol);
  value->_enum.members = genList(mirgen, &node->_enum.members, enum_symbol);
  value->_enum.definitions = genList(mirgen, &node->_enum.body, enum_symbol);
  return value;
}

MIRValue *genUnion(MIRGen *mirgen, Node *node, Symbol *scope) {
  Symbol *union_symbol = scope->findSymbolByNode(node);

  MIRValue *value = mirgen->ctx->make({.kind = MIRValueKind::Union});
  value->_union.repr_type = gen(mirgen, node->_union.repr_type, union_symbol);
  value->_union.members = genList(mirgen, &node->_union.variants, union_symbol);
  value->_union.definitions = genList(mirgen, &node->_union.body, union_symbol);
  return value;
}

MIRValue *genNamespace(MIRGen *mirgen, Node *node, Symbol *scope) {
  Symbol *namespace_symbol = scope->findSymbolByNode(node);

  MIRValue *value = mirgen->ctx->make({.kind = MIRValueKind::Namespace});
  value->_namespace.definitions =
      genList(mirgen, &node->children, namespace_symbol);
  return value;
}
