#include "define.hpp"
#include "uir/literal.hpp"
#include "uir/uir.hpp"

void genList(UIRGen *uirgen, ArrayList<Node *> *list, Symbol *scope,
             UIRScope *out) {
  out->list.init(uirgen->module->allocator, list->length);

  UIRBlock *prev_block = uirgen->builder.block;
  UIRScope *prev_scope = uirgen->builder.scope;
  uirgen->builder.block = nullptr;
  uirgen->builder.scope = out;

  // Generate Declarations
  for (size_t i = 0; i < list->length; i++) {
    genDeclaration(uirgen, list->getUnchecked(i), scope);
  }

  // Generate Definitions
  for (size_t i = 0; i < list->length; i++) {
    gen(uirgen, list->getUnchecked(i), scope);
  }

  uirgen->builder.scope = prev_scope;
  uirgen->builder.block = prev_block;
}

UIRValue *genStruct(UIRGen *uirgen, Node *node, Symbol *scope) {
  Symbol *struct_symbol = scope->findSymbolByNode(node);

  // Fields
  Slice<UIRStruct::Field> fields = {
      .ptr = (UIRStruct::Field *)uirgen->module->allocator->alloc(
          sizeof(UIRStruct::Field) * node->_struct.fields.length),
      .len = node->_struct.fields.length,
  };

  for (size_t i = 0; i < node->_struct.fields.length; i++) {
    Node *ast = node->_struct.fields.getUnchecked(i);
    UIRStruct::Field *uir = fields.ptr + i;
    uir->name = ast->field.name;
    uir->type = gen(uirgen, ast->field.type, struct_symbol);
  }

  // Build Instruction
  UIRValue *value = uirgen->builder.buildStruct(fields, {.ptr = nullptr});
  value->source_location = node->location;

  // Definitions
  value->_struct.definitions =
      (UIRScope *)uirgen->module->allocator->alloc(sizeof(UIRScope));
  genList(uirgen, &node->_struct.body, struct_symbol,
          value->_struct.definitions);

  return value;
}

UIRValue *genEnum(UIRGen *uirgen, Node *node, Symbol *scope) {
  Symbol *enum_symbol = scope->findSymbolByNode(node);

  UIRValue *repr_type;
  if (node->_enum.repr_type != nullptr) {
    repr_type = gen(uirgen, node->_enum.repr_type, enum_symbol);
  } else {
    UIRLiteral literal = {
        .lit_type = uirgen->ctx->type_cache->get({.kind = TypeKind::TypeId}),
        .kind = UIRLiteralKind::Typed,
        ._typeid = uirgen->ctx->type_cache->get({
            .kind = TypeKind::Integer,
            .integer = {false, false, 32},
            .is_constant = true,
        })};
    repr_type = uirgen->builder.buildLiteral(literal);
  }

  // Members
  Slice<UIREnum::Member> members = {
      .ptr = (UIREnum::Member *)uirgen->module->allocator->alloc(
          sizeof(UIREnum::Member) * node->_enum.members.length),
      .len = node->_enum.members.length,
  };

  for (size_t i = 0; i < node->_enum.members.length; i++) {
    Node *ast = node->_enum.members.getUnchecked(i);
    UIREnum::Member *uir = members.ptr + i;
    uir->name = ast->member.name;
    if (ast->member.value != nullptr) {
      uir->constant = gen(uirgen, ast->member.value, enum_symbol);
    } else {
      uir->constant = nullptr;
    }
  }

  // Build Instruction
  UIRValue *value =
      uirgen->builder.buildEnum(repr_type, members, {.ptr = nullptr});
  value->source_location = node->location;

  // Definitions
  value->_enum.definitions =
      (UIRScope *)uirgen->module->allocator->alloc(sizeof(UIRScope));
  genList(uirgen, &node->_enum.body, enum_symbol, value->_enum.definitions);

  return value;
}

UIRValue *genUnion(UIRGen *uirgen, Node *node, Symbol *scope) {
  Symbol *union_symbol = scope->findSymbolByNode(node);

  UIRValue *repr_type = gen(uirgen, node->_union.repr_type, union_symbol);

  // Variants
  Slice<UIRStruct::Field> variants = {
      .ptr = (UIRStruct::Field *)uirgen->module->allocator->alloc(
          sizeof(UIRStruct::Field) * node->_union.variants.length),
      .len = node->_union.variants.length,
  };

  for (size_t i = 0; i < node->_union.variants.length; i++) {
    Node *ast = node->_union.variants.getUnchecked(i);
    UIRStruct::Field *uir = variants.ptr + i;
    uir->name = ast->field.name;
    uir->type = gen(uirgen, ast->field.type, union_symbol);
  }

  // Build Instruction
  UIRValue *value =
      uirgen->builder.buildUnion(repr_type, variants, {.ptr = nullptr});
  value->source_location = node->location;

  // Definitions
  value->_union.definitions =
      (UIRScope *)uirgen->module->allocator->alloc(sizeof(UIRScope));
  genList(uirgen, &node->_union.body, union_symbol, value->_union.definitions);

  return value;
}

UIRValue *genNamespace(UIRGen *uirgen, Node *node, Symbol *scope) {
  Symbol *namespace_symbol = scope->findSymbolByNode(node);

  UIRValue *value = uirgen->builder.buildNamespace({.ptr = nullptr});
  value->source_location = node->location;
  value->_namespace.definitions =
      (UIRScope *)uirgen->module->allocator->alloc(sizeof(UIRScope));

  genList(uirgen, &node->children, namespace_symbol,
          value->_namespace.definitions);
  return value;
}
