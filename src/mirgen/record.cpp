#include "define.hpp"
#include "mir.hpp"

void genList(MIRGen *mirgen, ArrayList<Node *> *list, Symbol *scope,
             MIRScopeId out) {
  Option<MIRBlockId> prev_block = mirgen->builder.block;
  Option<MIRScopeId> prev_scope = mirgen->builder.scope;
  mirgen->builder.block.setNone();
  mirgen->builder.scope.setSome(out);

  // Generate Declarations
  for (size_t i = 0; i < list->length; i++) {
    genDeclaration(mirgen, list->getUnchecked(i), scope);
  }

  // Generate Definitions
  for (size_t i = 0; i < list->length; i++) {
    gen(mirgen, list->getUnchecked(i), scope);
  }

  mirgen->builder.scope = prev_scope;
  mirgen->builder.block = prev_block;
}

MIRValueId genStruct(MIRGen *mirgen, Node *node, Symbol *scope) {
  Symbol *struct_symbol = scope->findSymbolByNode(node);

  // Fields
  Slice<MIRStruct::Field> fields = {
      .ptr = (MIRStruct::Field *)mirgen->module.arena.alloc(
          sizeof(MIRStruct::Field) * node->_struct.fields.length),
      .len = node->_struct.fields.length,
  };

  for (size_t i = 0; i < node->_struct.fields.length; i++) {
    Node *ast = node->_struct.fields.getUnchecked(i);
    MIRStruct::Field *mir = fields.ptr + i;
    mir->name = ast->field.name;
    mir->type = gen(mirgen, ast->field.type, struct_symbol);
  }

  // Build Instruction
  MIRValueId value = mirgen->builder.buildStruct(fields, {.ptr = nullptr});
  mirgen->builder.setSourceLocation(value, node->location);

  // Definitions
  MIRScopeId definitions = mirgen->builder.makeScope(value);
  mirgen->symbol_to_scope.insert(struct_symbol, definitions);
  genList(mirgen, &node->_struct.body, struct_symbol, definitions);
  mirgen->module.getInstr(value)->_struct.definitions = definitions;

  return value;
}

MIRValueId genEnum(MIRGen *mirgen, Node *node, Symbol *scope) {
  Symbol *enum_symbol = scope->findSymbolByNode(node);

  MIRValueId repr_type = gen(mirgen, node->_enum.repr_type, enum_symbol);

  // Members
  Slice<MIREnum::Member> members = {
      .ptr = (MIREnum::Member *)mirgen->module.arena.alloc(
          sizeof(MIREnum::Member) * node->_enum.members.length),
      .len = node->_enum.members.length,
  };

  for (size_t i = 0; i < node->_enum.members.length; i++) {
    Node *ast = node->_enum.members.getUnchecked(i);
    MIREnum::Member *mir = members.ptr + i;
    mir->name = ast->member.name;
    mir->constant = gen(mirgen, ast->member.value, enum_symbol);
  }

  // Build Instruction
  MIRValueId value =
      mirgen->builder.buildEnum(repr_type, members, {.ptr = nullptr});
  mirgen->builder.setSourceLocation(value, node->location);

  // Definitions
  MIRScopeId definitions = mirgen->builder.makeScope(value);
  mirgen->symbol_to_scope.insert(enum_symbol, definitions);
  genList(mirgen, &node->_enum.body, enum_symbol, definitions);
  mirgen->module.getInstr(value)->_enum.definitions = definitions;

  return value;
}

MIRValueId genUnion(MIRGen *mirgen, Node *node, Symbol *scope) {
  Symbol *union_symbol = scope->findSymbolByNode(node);

  MIRValueId repr_type = gen(mirgen, node->_union.repr_type, union_symbol);

  // Variants
  Slice<MIRStruct::Field> variants = {
      .ptr = (MIRStruct::Field *)mirgen->module.arena.alloc(
          sizeof(MIRStruct::Field) * node->_union.variants.length),
      .len = node->_union.variants.length,
  };

  for (size_t i = 0; i < node->_union.variants.length; i++) {
    Node *ast = node->_union.variants.getUnchecked(i);
    MIRStruct::Field *mir = variants.ptr + i;
    mir->name = ast->field.name;
    mir->type = gen(mirgen, ast->field.type, union_symbol);
  }

  // Build Instruction
  MIRValueId value =
      mirgen->builder.buildUnion(repr_type, variants, {.ptr = nullptr});
  mirgen->builder.setSourceLocation(value, node->location);

  // Definitions
  MIRScopeId definitions = mirgen->builder.makeScope(value);
  mirgen->symbol_to_scope.insert(union_symbol, definitions);
  genList(mirgen, &node->_union.body, union_symbol, definitions);
  mirgen->module.getInstr(value)->_union.definitions = definitions;

  return value;
}

MIRValueId genNamespace(MIRGen *mirgen, Node *node, Symbol *scope) {
  Symbol *namespace_symbol = scope->findSymbolByNode(node);

  MIRValueId value = mirgen->builder.buildNamespace({.ptr = nullptr});
  mirgen->builder.setSourceLocation(value, node->location);

  MIRScopeId definitions = mirgen->builder.makeScope(value);
  mirgen->symbol_to_scope.insert(namespace_symbol, definitions);
  genList(mirgen, &node->children, namespace_symbol, definitions);

  return value;
}
