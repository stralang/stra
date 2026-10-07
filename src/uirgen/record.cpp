#include "define.hpp"
#include "uir/literal.hpp"
#include "uir/uir.hpp"

void genList(UIRGen *uirgen, ArrayList<Node *> *list, Symbol *scope,
             UIRScope *out) {
  out->list.init(uirgen->module->allocator, list->length);

  Option<UIRBlock *> prev_block = uirgen->builder.block;
  Option<UIRScope *> prev_scope = uirgen->builder.scope;
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
    uir->type = gen(uirgen, ast->field.type, struct_symbol)->id;
  }

  // Build Instruction
  UIRValue **cached = uirgen->node_to_value.get(node);
  UIRValue *value;
  if (cached != nullptr) {
    value = *cached;
    value->_struct.fields = fields;
  } else {
    value = uirgen->builder.buildStruct(fields, {.ptr = nullptr});
  }
  value->source_location = node->location;

  // Definitions
  UIRScope *definitions = uirgen->builder.createScope();
  value->_struct.definitions = definitions->id;
  genList(uirgen, &node->_struct.body, struct_symbol, definitions);

  return value;
}

UIRValue *genEnum(UIRGen *uirgen, Node *node, Symbol *scope) {
  Symbol *enum_symbol = scope->findSymbolByNode(node);

  UIRValue *repr_type;
  if (node->_enum.repr_type != nullptr) {
    repr_type = gen(uirgen, node->_enum.repr_type, enum_symbol);
  } else {
    UIRLiteral literal;
    literal.data = UIRRawData{
        .kind = UIRRawDataKind::TypeId,
        ._typeid = uirgen->ctx->types->push({
            .kind = RIRTypeKind::Integer,
            .integer = {false, 32},
        }),
    };
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
      uir->constant = gen(uirgen, ast->member.value, enum_symbol)->id;
    } else {
      uir->constant = {};
    }
  }

  // Build Instruction
  UIRValue **cached = uirgen->node_to_value.get(node);
  UIRValue *value;
  if (cached != nullptr) {
    value = *cached;
    value->_enum = {.repr_type = repr_type->id, .members = members};
  } else {
    value = uirgen->builder.buildEnum(repr_type->id, members, {.ptr = nullptr});
  }
  value->source_location = node->location;

  // Definitions
  UIRScope *definitions = uirgen->builder.createScope();
  value->_enum.definitions = definitions->id;
  genList(uirgen, &node->_enum.body, enum_symbol, definitions);

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
    uir->type = gen(uirgen, ast->field.type, union_symbol)->id;
  }

  // Build Instruction
  UIRValue **cached = uirgen->node_to_value.get(node);
  UIRValue *value;
  if (cached != nullptr) {
    value = *cached;
    value->_union = {.repr_type = repr_type->id, .variants = variants};
  } else {
    value =
        uirgen->builder.buildUnion(repr_type->id, variants, {.ptr = nullptr});
  }
  value->source_location = node->location;

  // Definitions
  UIRScope *definitions = uirgen->builder.createScope();
  value->_union.definitions = definitions->id;
  genList(uirgen, &node->_union.body, union_symbol, definitions);

  return value;
}

UIRValue *genNamespace(UIRGen *uirgen, Node *node, Symbol *scope) {
  Symbol *namespace_symbol = scope->findSymbolByNode(node);

  UIRValue **cached = uirgen->node_to_value.get(node);
  UIRValue *value;
  if (cached != nullptr) {
    value = *cached;
  } else {
    value = uirgen->builder.buildNamespace({.ptr = nullptr});
  }

  value->source_location = node->location;

  UIRScope *definitions = uirgen->builder.createScope();
  value->_namespace.definitions = definitions->id;
  genList(uirgen, &node->children, namespace_symbol, definitions);

  return value;
}
