#include "parser.hpp"
#include "ast.hpp"
#include "containers.hpp"
#include "operator.hpp"
#include "symbol.hpp"
#include "token.hpp"
#include <cassert>
#include <iostream>

#define try(is_ok)                                                             \
  if (!(is_ok)) {                                                              \
    return nullptr;                                                            \
  }

// Forward Declarations [
Node *parseExpr(ASTParser *parser, Precedence min_precedence, Scope *scope);
Node *parseField(ASTParser *parser, Node *name_prealloc, Scope *scope);
Node *parseStmtCompound(ASTParser *parser, Scope *scope);
// ] Forward Declarations

Node *parseBinaryExpr(ASTParser *parser, Node *atom, Precedence min_precedence,
                      Scope *scope) {
  Node *out = atom;

  while (parser->cur_token.kind == TokenKind::Operator) {
    Operator opcode = parser->cur_token._operator;
    Precedence precedence = operatorPrecedence(opcode);
    if (precedence < min_precedence) {
      break;
    }

    Associativity associativity = operatorAssociativity(opcode);
    precedence = (Precedence)((int32_t)precedence + (int32_t)associativity);

    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->kind = NodeKind::Operator;
    out->token = parser->cur_token;
    out->location = out->token.location;
    out->_operator.opcode = opcode;
    out->_operator.lhs = atom;

    try(parser->nextToken());
    out->_operator.rhs = parseExpr(parser, precedence, scope);
    try(out->_operator.rhs != nullptr);
  }

  return out;
}

Node *parsePartialExpr(ASTParser *parser, Precedence min_precedence, Node *atom,
                       Scope *scope) {
  Node *out = atom;

  switch (parser->cur_token.kind) {
  case TokenKind::ScopeBegin: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Call;
    out->call.callee = atom;
    out->call.arguments.init(parser->allocator, 4);

    try(parser->nextToken());
    while (parser->cur_token.kind != TokenKind::ScopeEnd) {
      Node *arg = parseExpr(parser, Precedence::Assign, scope);
      try(arg != nullptr);
      out->call.arguments.push(arg);

      if (parser->cur_token.kind != TokenKind::CommaDelimiter) {
        break;
      }
      try(parser->nextToken());
    }

    try(parser->cur_token.kind == TokenKind::ScopeEnd);
    try(parser->nextToken());

    try(out != nullptr);
    break;
  }
  case TokenKind::ArrayBegin: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Index;
    out->index.slice = atom;

    try(parser->nextToken());
    out->index.index = parseExpr(parser, Precedence::Assign, scope);

    try(parser->cur_token.kind == TokenKind::ArrayEnd);
    try(parser->nextToken());

    try(out != nullptr);
    break;
  }
  }

  if (parser->cur_token.kind == TokenKind::Operator) {
    out = parseBinaryExpr(parser, out, min_precedence, scope);
    try(out != nullptr);
  }

  return out;
}

struct FieldsAndBodyResult {
  bool ok;
  ArrayList<Node *> fields;
  ArrayList<Node *> body;
};

FieldsAndBodyResult parseFieldsAndBody(ASTParser *parser, Scope *scope) {
  ArrayList<Node *> fields;
  ArrayList<Node *> body;
  fields.init(parser->allocator, 8);
  body.init(parser->allocator, 8);

  bool allow_field = true;
  while (parser->cur_token.kind != TokenKind::BlockEnd) {
    Node *field = (Node *)parser->allocator->alloc(sizeof(Node));
    field->token = parser->cur_token;
    field->location = parser->cur_token.location;
    field->kind = NodeKind::Name;
    field->text = parser->cur_token.text;
    if (!parser->nextToken()) {
      return {false};
    }

    field = parseField(parser, field, scope);

    if (!field->field.definition) {
      if (!allow_field) {
        std::cerr << "Previous field didn't end with `,`\n";
        return {false};
      }

      fields.push(field);
      allow_field = parser->cur_token.kind == TokenKind::CommaDelimiter;
      if (allow_field && !parser->nextToken()) {
        return {false};
      }
      continue;
    }

    body.push(field);
    if (parser->cur_token.kind == TokenKind::LineDelimiter) {
      if (!parser->nextToken()) {
        return {false};
      }
    } else if (parser->prev_token.kind != TokenKind::BlockEnd) {
      std::cerr << "Statement must end with either `;` or `}`\n";
      return {false};
    }
  }

  return {true, fields, body};
}

FieldsAndBodyResult parseMembersAndBody(ASTParser *parser, Scope *scope) {
  ArrayList<Node *> fields;
  ArrayList<Node *> body;
  fields.init(parser->allocator, 8);
  body.init(parser->allocator, 8);

  bool allow_member = true;
  while (parser->cur_token.kind != TokenKind::BlockEnd) {
    Node *field = (Node *)parser->allocator->alloc(sizeof(Node));
    field->token = parser->cur_token;
    field->location = parser->cur_token.location;
    field->kind = NodeKind::Name;
    field->text = parser->cur_token.text;
    if (!parser->nextToken()) {
      return {false};
    }

    if (parser->cur_token.kind != TokenKind::TypeSeperator) {
      if (!allow_member) {
        std::cerr << "Previous member didn't end with `,`\n";
        return {false};
      }

      field->kind = NodeKind::Member;
      field->member.name = field->text;
      field->member.value = nullptr;

      if (parser->cur_token.kind == TokenKind::Operator &&
          parser->cur_token._operator == Operator::Assign) {
        if (!parser->nextToken()) {
          return {false};
        }
        field->member.value = parseExpr(parser, Precedence::Assign, scope);
      }

      fields.push(field);

      allow_member = parser->cur_token.kind == TokenKind::CommaDelimiter;
      if (allow_member && !parser->nextToken()) {
        return {false};
      }

      continue;
    }

    field = parseField(parser, field, scope);
    body.push(field);

    if (parser->cur_token.kind == TokenKind::LineDelimiter) {
      if (!parser->nextToken()) {
        return {false};
      }
    } else if (parser->prev_token.kind != TokenKind::BlockEnd) {
      std::cerr << "Statement must end with either `;` or `}`\n";
      return {false};
    }
  }

  return {true, fields, body};
}

Node *parseExpr(ASTParser *parser, Precedence min_precedence, Scope *scope) {
  Node *out;

  if (parser->cur_token.kind != TokenKind::ScopeBegin) {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
  }

  switch (parser->cur_token.kind) {
  case TokenKind::Operator: {
    // Parse Unary
    out->kind = NodeKind::UnaryOperator;
    out->unary_operator.opcode = (UnaryOperator)parser->cur_token._operator;

    try(parser->nextToken());
    out->unary_operator.child =
        parseExpr(parser, Precedence::MemberAccess, scope);
    try(out->unary_operator.child != nullptr);
    break;
  }
  case TokenKind::Name: {
    out->kind = NodeKind::Name;
    out->text = parser->cur_token.text;
    parser->nextToken();
    break;
  }
  case TokenKind::Integer: {
    out->kind = NodeKind::Integer;
    out->integer = parser->cur_token.integer;
    parser->nextToken();
    break;
  }
  case TokenKind::Float: {
    out->kind = NodeKind::Float;
    out->_float = parser->cur_token._float;
    parser->nextToken();
    break;
  }
  case TokenKind::Char: {
    out->kind = NodeKind::Char;
    out->integer = parser->cur_token.integer;
    parser->nextToken();
    break;
  }
  case TokenKind::String: {
    out->kind = NodeKind::String;
    out->text = parser->cur_token.text;
    parser->nextToken();
    break;
  }
  case TokenKind::Function: {
    out->kind = NodeKind::Function;
    out->function.parameters.init(parser->allocator, 8);
    out->function.return_type = nullptr;
    out->function.body = nullptr;
    out->function.undefined = false;

    try(parser->nextToken());

    // Parse Parameters
    try(parser->cur_token.kind == TokenKind::ScopeBegin);
    try(parser->nextToken());
    while (parser->cur_token.kind != TokenKind::ScopeEnd) {
      Node *parameter = (Node *)parser->allocator->alloc(sizeof(Node));
      parameter->token = parser->cur_token;
      parameter->location = parser->cur_token.location;
      parameter->kind = NodeKind::Name;
      parameter->text = parser->cur_token.text;
      try(parser->nextToken());

      parameter = parseField(parser, parameter, scope);
      out->function.parameters.push(parameter);

      if (parser->cur_token.kind != TokenKind::CommaDelimiter) {
        break;
      }
      try(parser->nextToken());
    }
    try(parser->cur_token.kind == TokenKind::ScopeEnd);
    try(parser->nextToken());

    // Parse Return
    if (parser->cur_token.kind != TokenKind::BlockBegin &&
        parser->cur_token.kind != TokenKind::Undefined &&
        parser->cur_token.kind != TokenKind::LineDelimiter) {
      out->function.return_type = parseExpr(parser, Precedence::Assign, scope);
      try(out->function.return_type != nullptr);
    }

    // Parse Body
    if (parser->cur_token.kind == TokenKind::BlockBegin) {
      out->function.body = parseStmtCompound(parser, scope);
      try(out->function.body != nullptr);
    } else if (parser->cur_token.kind == TokenKind::Undefined) {
      try(parser->nextToken());
      out->function.undefined = true;
    }

    break;
  }
  case TokenKind::Struct: {
    out->kind = NodeKind::Struct;
    try(parser->nextToken());
    try(parser->cur_token.kind == TokenKind::BlockBegin);
    try(parser->nextToken());

    // Create Scope
    Scope *record_scope = (Scope *)parser->allocator->alloc(sizeof(Scope));
    record_scope->init(parser->allocator, false, scope);
    record_scope->node = out;

    // Parse Body
    FieldsAndBodyResult result = parseFieldsAndBody(parser, record_scope);
    try(result.ok);
    out->_struct.fields = result.fields;
    out->_struct.body = result.body;

    try(parser->cur_token.kind == TokenKind::BlockEnd);
    parser->nextToken();
    break;
  }
  case TokenKind::Enum: {
    out->kind = NodeKind::Enum;
    out->_enum.repr_type = nullptr;

    try(parser->nextToken());
    if (parser->cur_token.kind != TokenKind::BlockBegin) {
      out->_enum.repr_type = parseExpr(parser, Precedence::Assign, scope);
    }

    try(parser->cur_token.kind == TokenKind::BlockBegin);
    try(parser->nextToken());

    // Create Scope
    Scope *record_scope = (Scope *)parser->allocator->alloc(sizeof(Scope));
    record_scope->init(parser->allocator, false, scope);
    record_scope->node = out;

    // Parse Body
    FieldsAndBodyResult result = parseMembersAndBody(parser, record_scope);
    try(result.ok);
    out->_enum.members = result.fields;
    out->_enum.body = result.body;

    try(parser->cur_token.kind == TokenKind::BlockEnd);
    parser->nextToken();
    break;
  }
  case TokenKind::Union: {
    out->kind = NodeKind::Union;
    out->_union.repr_type = nullptr;

    try(parser->nextToken());
    if (parser->cur_token.kind != TokenKind::BlockBegin) {
      out->_union.repr_type = parseExpr(parser, Precedence::Assign, scope);
    }

    try(parser->cur_token.kind == TokenKind::BlockBegin);
    try(parser->nextToken());

    // Create Scope
    Scope *record_scope = (Scope *)parser->allocator->alloc(sizeof(Scope));
    record_scope->init(parser->allocator, false, scope);
    record_scope->node = out;

    // Parse Body
    FieldsAndBodyResult result = parseFieldsAndBody(parser, record_scope);
    try(result.ok);
    out->_union.variants = result.fields;
    out->_union.body = result.body;

    try(parser->cur_token.kind == TokenKind::BlockEnd);
    parser->nextToken();
    break;
  }
  case TokenKind::Import: {
    out->kind = NodeKind::Import;
    try(parser->nextToken());
    try(parser->cur_token.kind == TokenKind::String);

    out->import.path = parser->cur_token.text;
    try(parser->nextToken());
    break;
  }
  case TokenKind::Comptime: {
    out->kind = NodeKind::Comptime;
    try(parser->nextToken());
    out->child = parseExpr(parser, Precedence::MemberAccess, scope);
    break;
  }
  case TokenKind::Const: {
    out->kind = NodeKind::Const;
    try(parser->nextToken());
    out->child = parseExpr(parser, Precedence::MemberAccess, scope);
    break;
    break;
  }
  case TokenKind::ArrayBegin: {
    out->kind = NodeKind::Slice;
    out->slice.is_pointer = false;
    out->slice.length = nullptr;
    out->slice.type = nullptr;

    try(parser->nextToken());
    if (parser->cur_token.kind == TokenKind::Operator &&
        parser->cur_token._operator == Operator::Mul) {
      out->slice.is_pointer = true;
      try(parser->nextToken());
    } else if (parser->cur_token.kind != TokenKind::ArrayEnd) {
      out->slice.length = parseExpr(parser, Precedence::Assign, scope);
    }

    try(parser->cur_token.kind == TokenKind::ArrayEnd);
    try(parser->nextToken());

    out->slice.type = parseExpr(parser, Precedence::Assign, scope);
    break;
  }
  case TokenKind::ScopeBegin: {
    try(parser->nextToken());
    out = parseExpr(parser, Precedence::Assign, scope);
    try(parser->cur_token.kind == TokenKind::ScopeEnd);
    try(parser->nextToken());
    break;
  }
  }

  out = parsePartialExpr(parser, min_precedence, out, scope);
  try(out != nullptr);

  return out;
}

// The `name_prealloc` argument must contain the name of the variable, and is
// used as a preallocated node for the output
Node *parseField(ASTParser *parser, Node *name_prealloc, Scope *scope) {
  Node *out = name_prealloc;
  try(out->kind == NodeKind::Name);

  out->kind = NodeKind::Field;
  out->field.name = out->text;
  out->field.definition = false;
  out->field.type = nullptr;
  out->field.initial = nullptr;

  // Create Symbol
  Symbol *symbol = (Symbol *)parser->allocator->alloc(sizeof(Symbol));
  symbol->parent = scope;
  symbol->node = out;
  scope->symbols.push(symbol);

  // Parse Type
  try(parser->cur_token.kind == TokenKind::TypeSeperator);
  try(parser->nextToken());
  if (parser->cur_token.kind != TokenKind::TypeSeperator &&
      (parser->cur_token.kind != TokenKind::Operator ||
       parser->cur_token._operator != Operator::Assign)) {
    out->field.type =
        parseExpr(parser, (Precedence)((int32_t)Precedence::Assign + 1), scope);
    try(out->field.type);
  }

  // Parse Initial
  out->field.definition = parser->cur_token.kind == TokenKind::TypeSeperator;
  if (out->field.definition ||
      (parser->cur_token.kind == TokenKind::Operator &&
       parser->cur_token._operator == Operator::Assign)) {
    try(parser->nextToken());

    out->field.undefined = parser->cur_token.kind == TokenKind::Undefined;
    if (!out->field.undefined) {
      out->field.initial = parseExpr(parser, Precedence::Assign, scope);
      try(out->field.initial);
    } else {
      try(parser->nextToken());
    }
  }

  return out;
}

Node *parseConditional(ASTParser *parser, Scope *scope) {
  Node *out = (Node *)parser->allocator->alloc(sizeof(Node));
  out->token = parser->cur_token;
  out->location = parser->cur_token.location;
  out->kind = NodeKind::Compound;
  out->children.init(parser->allocator, 2);

  while (parser->cur_token.kind != TokenKind::BlockBegin) {
    Node *child;

    if (parser->cur_token.kind == TokenKind::Name) {
      child = (Node *)parser->allocator->alloc(sizeof(Node));
      child->token = parser->cur_token;
      child->location = parser->cur_token.location;
      child->kind = NodeKind::Name;
      child->text = parser->cur_token.text;

      try(parser->nextToken());
      if (parser->cur_token.kind == TokenKind::TypeSeperator) {
        child = parseField(parser, child, scope);
      } else {
        child = parsePartialExpr(parser,
                                 (Precedence)((int32_t)Precedence::Assign + 1),
                                 child, scope);
      }
    } else {
      child = parseExpr(parser, Precedence::Assign, scope);
    }

    out->children.push(child);

    if (parser->cur_token.kind != TokenKind::LineDelimiter) {
      break;
    }
    try(parser->nextToken());
  }

  return out;
}

Node *parseAttribute(ASTParser *parser, Scope *scope) {
  Node *out = (Node *)parser->allocator->alloc(sizeof(Node));
  out->token = parser->cur_token;
  out->location = parser->cur_token.location;
  out->kind = NodeKind::Attribute;
  out->children.init(parser->allocator, 2);

  try(parser->nextToken());
  try(parser->cur_token.kind == TokenKind::ScopeBegin);
  try(parser->nextToken());

  while (parser->cur_token.kind != TokenKind::ScopeEnd) {
    try(parser->cur_token.kind == TokenKind::Name);

    Node *attribute = (Node *)parser->allocator->alloc(sizeof(Node));
    attribute->token = parser->cur_token;
    attribute->location = parser->cur_token.location;
    attribute->kind = NodeKind::Name;
    attribute->text = parser->cur_token.text;

    try(parser->nextToken());
    if (parser->cur_token.kind == TokenKind::Operator &&
        parser->cur_token._operator == Operator::Assign) {
      try(parser->nextToken());
      attribute->kind = NodeKind::Member;
      attribute->member.name = attribute->text;
      attribute->member.value = parseExpr(parser, Precedence::Assign, scope);
    }

    out->children.push(attribute);

    if (parser->cur_token.kind != TokenKind::CommaDelimiter) {
      break;
    }
    try(parser->nextToken());
  }

  try(parser->cur_token.kind == TokenKind::ScopeEnd);
  parser->nextToken();
  return out;
}

Node *parseStmt(ASTParser *parser, Scope *scope) {
  Node *out = nullptr;

  switch (parser->cur_token.kind) {
  case TokenKind::Name: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = out->token.location;
    out->kind = NodeKind::Name;
    out->text = out->token.text;

    try(parser->nextToken());
    if (parser->cur_token.kind == TokenKind::TypeSeperator) {
      out = parseField(parser, out, scope);
    } else {
      out = parsePartialExpr(parser, Precedence::Assign, out, scope);
    }
    break;
  }
  case TokenKind::Operator: {
    out = parseExpr(parser, Precedence::Assign, scope);
    break;
  }
  case TokenKind::Return: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Return;
    out->child = nullptr;

    try(parser->nextToken());
    if (parser->cur_token.kind != TokenKind::LineDelimiter) {
      out->child = parseExpr(parser, Precedence::Assign, scope);
    }
    break;
  }
  case TokenKind::If: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::If;

    // Create Scope
    Scope *if_scope = (Scope *)parser->allocator->alloc(sizeof(Scope));
    if_scope->init(parser->allocator, true, scope);
    if_scope->node = out;

    // Parse Conditional
    try(parser->nextToken());
    out->_if.conditional = parseConditional(parser, if_scope);
    try(parser->cur_token.kind == TokenKind::BlockBegin);

    // Parse Body
    out->_if.body = parseStmtCompound(parser, if_scope);
    out->_if._else = nullptr;

    // Parse Else and Else-If
    if (parser->cur_token.kind == TokenKind::Else) {
      try(parser->nextToken());

      if (parser->cur_token.kind == TokenKind::If) {
        out->_if._else = parseStmt(parser, scope);
      } else if (parser->cur_token.kind == TokenKind::BlockBegin) {
        // Create Scope
        Scope *else_scope = (Scope *)parser->allocator->alloc(sizeof(Scope));
        else_scope->init(parser->allocator, true, scope);

        // Parse Body
        out->_if._else = parseStmtCompound(parser, else_scope);
        else_scope->node = out->_if._else;
      } else {
        std::cerr << "Else body must be either an if statement or compound\n";
        return nullptr;
      }
    }

    break;
  }
  case TokenKind::For: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::For;

    // Create Scope
    Scope *for_scope = (Scope *)parser->allocator->alloc(sizeof(Scope));
    for_scope->init(parser->allocator, true, scope);
    for_scope->node = out;

    // Parse Conditional
    try(parser->nextToken());
    out->_for.conditional = parseConditional(parser, for_scope);
    try(parser->cur_token.kind == TokenKind::BlockBegin);

    // Parse body
    out->_for.body = parseStmtCompound(parser, scope);
    break;
  }
  case TokenKind::Switch: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Switch;
    out->_switch.cases.init(parser->allocator, 8);

    try(parser->nextToken());
    out->_switch.conditional = parseExpr(parser, Precedence::Assign, scope);

    try(parser->cur_token.kind == TokenKind::BlockBegin);
    try(parser->nextToken());

    while (parser->cur_token.kind != TokenKind::BlockEnd) {
      Node *_case = (Node *)parser->allocator->alloc(sizeof(Node));
      _case->kind = NodeKind::Case;

      // Parse Constant
      _case->_case.constant = parseExpr(parser, Precedence::Assign, scope);
      try(parser->cur_token.kind == TokenKind::Case);
      _case->token = parser->cur_token;
      _case->location = parser->cur_token.location;

      try(parser->nextToken());

      // Create Scope
      Scope *case_scope = (Scope *)parser->allocator->alloc(sizeof(Scope));
      case_scope->init(parser->allocator, true, scope);

      // Parse Body
      _case->_case.body = parseStmtCompound(parser, case_scope);
      out->_switch.cases.push(_case);
      case_scope->node = _case->_case.body;
    }

    try(parser->cur_token.kind == TokenKind::BlockEnd);
    parser->nextToken();

    break;
  }
  case TokenKind::Break: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Break;
    try(parser->nextToken());
    break;
  }
  case TokenKind::Continue: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Continue;
    try(parser->nextToken());
    break;
  }
  case TokenKind::Defer: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Defer;

    try(parser->nextToken());
    out->child = parseStmt(parser, scope);
    break;
  }
  case TokenKind::Comptime: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Comptime;

    try(parser->nextToken());
    out->child = parseStmt(parser, scope);
    break;
  }
  case TokenKind::Assembly: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Assembly;
    out->assembly.instructions.init(parser->allocator, 8);

    try(parser->nextToken());
    try(parser->cur_token.kind == TokenKind::BlockBegin);
    try(parser->nextToken());

    // Parse Instructions
    while (parser->cur_token.kind != TokenKind::BlockEnd) {
      try(parser->cur_token.kind == TokenKind::Name);

      NodeAssembly::Instruction inst;
      inst.token = parser->cur_token;
      inst.location = parser->cur_token.location;
      inst.name = parser->cur_token.text;
      inst.arguments.init(parser->allocator, 4);

      // Parse Arguments
      try(parser->nextToken());
      while (parser->cur_token.kind != TokenKind::LineDelimiter) {
        NodeAssembly::Argument arg;
        arg.token = parser->cur_token;
        arg.location = parser->cur_token.location;
        arg.kind = NodeAssembly::Argument::Input;

        if (parser->cur_token.kind == TokenKind::Operator &&
            parser->cur_token._operator == Operator::Assign) {
          try(parser->nextToken());
          arg.kind = NodeAssembly::Argument::Return;
        }

        if (parser->cur_token.kind == TokenKind::Operator &&
            parser->cur_token._operator == Operator::Mod) {
          try(parser->nextToken());
          arg.kind = NodeAssembly::Argument::Register;
          arg.reg = parser->cur_token.text;
          try(parser->nextToken());
        } else {
          arg.node = parseExpr(parser, Precedence::Assign, scope);
        }

        inst.arguments.push(arg);
        if (parser->cur_token.kind != TokenKind::CommaDelimiter) {
          break;
        }
        try(parser->nextToken());
      }

      out->assembly.instructions.push(inst);
      try(parser->cur_token.kind == TokenKind::LineDelimiter);
      try(parser->nextToken());
    }

    try(parser->cur_token.kind == TokenKind::BlockEnd);
    try(parser->nextToken());
    break;
  }
  case TokenKind::BlockBegin: {
    // Create Scope
    Scope *child_scope = (Scope *)parser->allocator->alloc(sizeof(Scope));
    child_scope->init(parser->allocator, true, scope);

    // Parse Body
    out = parseStmtCompound(parser, child_scope);
    child_scope->node = out;
    break;
  }
  case TokenKind::Attribute: {
    out = parseAttribute(parser, scope);
    try(out);
    return out; // attributes don't end with `;` nor `}`
  }
  }

  try(out);
  if (parser->cur_token.kind == TokenKind::LineDelimiter) {
    parser->nextToken();
  } else if (parser->prev_token.kind != TokenKind::LineDelimiter &&
             parser->prev_token.kind != TokenKind::BlockEnd) {
    std::cerr << "Statement must end with either `;` or `}`\n";
    return nullptr;
  }

  return out;
}

// The passed `scope` should be preallocated for this node
Node *parseStmtCompound(ASTParser *parser, Scope *scope) {
  Node *out = (Node *)parser->allocator->alloc(sizeof(Node));
  out->kind = NodeKind::Compound;
  out->token = parser->cur_token;
  out->location = parser->cur_token.location;
  out->children.init(parser->allocator, 8);

  // Pass Marker
  if (parser->cur_token.kind == TokenKind::BlockBegin) {
    try(parser->nextToken());
  }

  // Parse Compound
  while (true) {
    Node *child = parseStmt(parser, scope);
    if (child == nullptr) {
      break;
    }

    out->children.push(child);
  }

  // Pass Marker
  if (parser->cur_token.kind == TokenKind::BlockEnd) {
    parser->nextToken();
  }

  return out;
}

void ASTParser::parse() {
  if (this->nextToken() == false) {
    this->ast = nullptr;
    return;
  }

  this->scope = (Scope *)this->allocator->alloc(sizeof(Scope));
  this->scope->location_aware = false;
  this->scope->children.init(this->allocator, 8);
  this->scope->symbols.init(this->allocator, 8);
  this->scope->parent = nullptr;

  this->comments.init(this->allocator, 16);
  this->ast = parseStmtCompound(this, this->scope);
  this->scope->node = this->ast;
}

bool ASTParser::nextToken() {
  Token next_token = this->tokenizer.next();
  while (next_token.kind == TokenKind::Comment) {
    this->comments.push(next_token);
    next_token = this->tokenizer.next();
  }

  this->prev_token = this->cur_token;
  this->cur_token = next_token;
  return this->cur_token.kind != TokenKind::Eof;
}
