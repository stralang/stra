#include "helper.hpp"
#include "symbol.hpp"

Node *getAttribute(Node *attributes, String name) {
  for (size_t i = 0; i < attributes->children.length; i++) {
    Node *child = attributes->children.data.ptr[i];
    if (child->member.name.compare(name)) {
      return child;
    }
  }

  return nullptr;
}

Node *astCopy(Allocator *allocator, Node *src, Symbol *scope) {
  if (src == nullptr) {
    return nullptr;
  }

  Node *dst = (Node *)allocator->alloc(sizeof(Node));
  dst->token = src->token;
  dst->location = src->location;
  dst->end_location = src->end_location;
  dst->doc_comments = astCopy(allocator, src->doc_comments, scope);
  dst->line_comments = astCopy(allocator, src->line_comments, scope);
  dst->kind = src->kind;

  // copying the value breaks validation
  dst->value = {.type = nullptr, .has_data = false};

  switch (src->kind) {
  case NodeKind::Namespace: {
    Symbol *new_scope = (Symbol *)allocator->alloc(sizeof(Symbol));
    new_scope->init(allocator, false, scope);
    new_scope->node = dst;
    scope = new_scope;
  }
  case NodeKind::Compound:
  case NodeKind::Attribute: {
    dst->children.init(allocator, src->children.length);
    for (size_t i = 0; i < src->children.length; i++) {
      dst->children.push(astCopy(allocator, src->children.data.ptr[i], scope));
    }
    break;
  }
  case NodeKind::Const:
  case NodeKind::Return:
  case NodeKind::Defer:
  case NodeKind::Comptime: {
    dst->child = astCopy(allocator, src->child, scope);
    break;
  }
  case NodeKind::Name:
  case NodeKind::RawString: {
    dst->text = src->text;
    break;
  }
  case NodeKind::Value: {
    dst->value = src->value;
    break;
  }
  case NodeKind::Field: {
    Symbol *symbol = (Symbol *)allocator->alloc(sizeof(Symbol));
    symbol->init(allocator, false, scope);
    symbol->node = dst;

    dst->field.name = src->field.name;
    dst->field.type = astCopy(allocator, src->field.type, symbol);
    dst->field.initial = astCopy(allocator, src->field.initial, symbol);
    dst->field.attributes = astCopy(allocator, src->field.attributes, symbol);
    dst->field.definition = src->field.definition;
    dst->field.undefined = src->field.undefined;
    break;
  }
  case NodeKind::Function: {
    Symbol *fn_scope = (Symbol *)allocator->alloc(sizeof(Symbol));
    fn_scope->init(allocator, true, scope);
    fn_scope->node = dst;

    dst->function.parameters.init(allocator, src->function.parameters.length);
    for (size_t i = 0; i < src->function.parameters.length; i++) {
      dst->function.parameters.push(
          astCopy(allocator, src->function.parameters.data.ptr[i], fn_scope));
    }
    dst->function.return_type =
        astCopy(allocator, src->function.return_type, scope);
    dst->function.body = astCopy(allocator, src->function.body, fn_scope);
    dst->function.undefined = src->function.undefined;
    break;
  }
  case NodeKind::Struct: {
    Symbol *record_scope = (Symbol *)allocator->alloc(sizeof(Symbol));
    record_scope->init(allocator, false, scope);
    record_scope->node = dst;

    dst->_struct.fields.init(allocator, src->_struct.fields.length);
    for (size_t i = 0; i < src->_struct.fields.length; i++) {
      dst->_struct.fields.push(
          astCopy(allocator, src->_struct.fields.data.ptr[i], record_scope));
    }

    dst->_struct.body.init(allocator, src->_struct.body.length);
    for (size_t i = 0; i < src->_struct.body.length; i++) {
      dst->_struct.body.push(
          astCopy(allocator, src->_struct.body.data.ptr[i], record_scope));
    }

    break;
  }
  case NodeKind::Enum: {
    Symbol *record_scope = (Symbol *)allocator->alloc(sizeof(Symbol));
    record_scope->init(allocator, false, scope);
    record_scope->node = dst;

    dst->_enum.repr_type = astCopy(allocator, src->_enum.repr_type, scope);

    dst->_enum.members.init(allocator, src->_enum.members.length);
    for (size_t i = 0; i < src->_enum.members.length; i++) {
      dst->_enum.members.push(
          astCopy(allocator, src->_enum.members.data.ptr[i], record_scope));
    }

    dst->_enum.body.init(allocator, src->_enum.body.length);
    for (size_t i = 0; i < src->_enum.body.length; i++) {
      dst->_enum.body.push(
          astCopy(allocator, src->_enum.body.data.ptr[i], record_scope));
    }

    break;
  }
  case NodeKind::Union: {
    Symbol *record_scope = (Symbol *)allocator->alloc(sizeof(Symbol));
    record_scope->init(allocator, false, scope);
    record_scope->node = dst;

    dst->_union.repr_type = astCopy(allocator, src->_union.repr_type, scope);

    dst->_union.variants.init(allocator, src->_union.variants.length);
    for (size_t i = 0; i < src->_union.variants.length; i++) {
      dst->_union.variants.push(
          astCopy(allocator, src->_union.variants.data.ptr[i], record_scope));
    }

    dst->_union.body.init(allocator, src->_union.body.length);
    for (size_t i = 0; i < src->_union.body.length; i++) {
      dst->_union.body.push(
          astCopy(allocator, src->_union.body.data.ptr[i], record_scope));
    }

    break;
  }
  case NodeKind::Member: {
    dst->member.name = src->member.name;
    dst->member.value = astCopy(allocator, src->member.value, scope);
    break;
  }
  case NodeKind::Import: {
    dst->import.path = src->import.path;
    dst->import.scope = src->import.scope;
    break;
  }
  case NodeKind::Slice: {
    dst->slice.is_pointer = src->slice.is_pointer;
    dst->slice.length = astCopy(allocator, src->slice.length, scope);
    dst->slice.type = astCopy(allocator, src->slice.type, scope);
    break;
  }
  case NodeKind::UnaryOperator: {
    dst->unary_operator.opcode = src->unary_operator.opcode;
    dst->unary_operator.child =
        astCopy(allocator, src->unary_operator.child, scope);
    break;
  }
  case NodeKind::Operator: {
    dst->_operator.opcode = src->_operator.opcode;
    dst->_operator.lhs = astCopy(allocator, src->_operator.lhs, scope);
    dst->_operator.rhs = astCopy(allocator, src->_operator.rhs, scope);
    break;
  }
  case NodeKind::Call: {
    dst->call.callee = astCopy(allocator, src->call.callee, scope);

    dst->call.arguments.init(allocator, src->call.arguments.length);
    for (size_t i = 0; i < src->call.arguments.length; i++) {
      dst->call.arguments.push(
          astCopy(allocator, src->call.arguments.data.ptr[i], scope));
    }
    break;
  }
  case NodeKind::Index: {
    dst->index.slice = astCopy(allocator, src->index.slice, scope);
    dst->index.index = astCopy(allocator, src->index.index, scope);
    break;
  }
  case NodeKind::Initializer: {
    dst->initializer.record =
        astCopy(allocator, src->initializer.record, scope);

    dst->initializer.setters.init(allocator, src->initializer.setters.length);
    for (size_t i = 0; i < src->initializer.setters.length; i++) {
      dst->initializer.setters.push(
          astCopy(allocator, src->initializer.setters.data.ptr[i], scope));
    }

    dst->initializer.is_list = src->initializer.is_list;
    break;
  }
  case NodeKind::If: {
    Symbol *then_scope = (Symbol *)allocator->alloc(sizeof(Symbol));
    then_scope->init(allocator, true, scope);
    then_scope->node = dst;

    dst->_if.conditional = astCopy(allocator, src->_if.conditional, then_scope);
    dst->_if.body = astCopy(allocator, src->_if.body, then_scope);
    dst->_if._else = nullptr;

    if (src->_if._else != nullptr) {
      Symbol *else_scope = scope;
      if (src->_if._else->kind != NodeKind::If) {
        else_scope = (Symbol *)allocator->alloc(sizeof(Symbol));
        else_scope->init(allocator, true, scope);
      }

      dst->_if._else = astCopy(allocator, src->_if._else, else_scope);

      if (else_scope != scope) {
        else_scope->node = dst->_if._else;
      }
    }
    break;
  }
  case NodeKind::For: {
    Symbol *for_scope = (Symbol *)allocator->alloc(sizeof(Symbol));
    for_scope->init(allocator, true, scope);
    for_scope->node = dst;

    dst->_for.conditional = astCopy(allocator, src->_if.conditional, for_scope);
    dst->_for.body = astCopy(allocator, src->_if.body, for_scope);
    break;
  }
  case NodeKind::Switch: {
    dst->_switch.conditional =
        astCopy(allocator, src->_switch.conditional, scope);

    dst->_switch.cases.init(allocator, src->_switch.cases.length);
    for (size_t i = 0; i < src->_switch.cases.length; i++) {
      Symbol *case_scope = (Symbol *)allocator->alloc(sizeof(Symbol));
      case_scope->init(allocator, true, scope);

      Node *case_dst =
          astCopy(allocator, src->_switch.cases.data.ptr[i], case_scope);
      case_scope->node = case_dst;
      dst->_switch.cases.push(case_dst);
    }
    break;
  }
  case NodeKind::Case: {
    dst->_case.constant = astCopy(allocator, src->_case.constant, scope);
    dst->_case.body = astCopy(allocator, src->_case.body, scope);
    break;
  }
  case NodeKind::Break:
  case NodeKind::Continue: {
    break;
  }
  case NodeKind::Assembly: {
    dst->assembly.instructions.init(allocator,
                                    src->assembly.instructions.length);
    for (size_t i = 0; i < src->assembly.instructions.length; i++) {
      NodeAssembly::Instruction *src_inst =
          src->assembly.instructions.data.ptr + i;
      NodeAssembly::Instruction dst_inst;
      dst_inst.token = src_inst->token;
      dst_inst.location = src_inst->location;
      dst_inst.name = src_inst->name;

      dst_inst.arguments.init(allocator, src_inst->arguments.length);
      for (size_t a = 0; a < src_inst->arguments.length; a++) {
        NodeAssembly::Argument *src_arg = src_inst->arguments.data.ptr + i;
        NodeAssembly::Argument dst_arg;
        dst_arg.token = src_arg->token;
        dst_arg.location = src_arg->location;
        dst_arg.kind = src_arg->kind;

        if (src_arg->kind == NodeAssembly::Argument::Register) {
          dst_arg.reg = src_arg->reg;
        } else {
          dst_arg.node = astCopy(allocator, src_arg->node, scope);
        }
      }

      src->assembly.instructions.push(dst_inst);
    }
    break;
  }
  case NodeKind::CommentGroup: {
    dst->comment_group.init(allocator, src->comment_group.length);
    for (size_t i = 0; i < src->comment_group.length; i++) {
      dst->comment_group.push(src->comment_group.data.ptr[i]);
    }
    break;
  }
  }

  return dst;
}

std::string replaceAll(std::string haystack, std::string needle,
                       std::string to) {
  if (needle.empty()) {
    return haystack;
  }

  size_t pos = 0;
  while (true) {
    pos = haystack.find(needle);
    if (pos == std::string::npos) {
      break;
    }

    haystack.replace(pos, needle.length(), to);
    pos += to.length();
  }

  return haystack;
}
