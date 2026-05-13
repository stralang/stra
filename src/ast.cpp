#include "ast.hpp"
#include "arraylist.hpp"
#include "operator.hpp"
#include "print.hpp"
#include "token.hpp"
#include <cassert>
#include <iostream>

#define try(is_ok)                                                             \
  if (!(is_ok)) {                                                              \
    return nullptr;                                                            \
  }

// Forward Declarations [
Node *parseExpr(ASTParser *parser, Precedence min_precedence);
Node *parseField(ASTParser *parser, Node *name_prealloc);
Node *parseStmtCompound(ASTParser *parser);
// ] Forward Declarations

Node *parseBinaryExpr(ASTParser *parser, Node *atom,
                      Precedence min_precedence) {
  Node *out = atom;

  while (parser->cur_token.kind == TokenKind::Operator) {
    Operator opcode = parser->cur_token._operator;
    Precedence precedence = operatorPrecedence(opcode);
    if (precedence < min_precedence) {
      break;
    }

    Associativity associativity = operatorAssociativity(opcode);
    precedence = (Precedence)((int32_t)precedence + (int32_t)associativity);

    out = (Node *)parser->allocator.alloc(sizeof(Node));
    out->kind = NodeKind::Operator;
    out->token = parser->cur_token;
    out->location = out->token.location;
    out->_operator.opcode = opcode;
    out->_operator.lhs = atom;

    try(parser->nextToken());
    out->_operator.rhs = parseExpr(parser, precedence);
    try(out->_operator.rhs != nullptr);
  }

  return out;
}

Node *parsePartialExpr(ASTParser *parser, Precedence min_precedence,
                       Node *atom) {
  Node *out = atom;

  switch (parser->cur_token.kind) {
  case TokenKind::Operator: {
    out = parseBinaryExpr(parser, out, min_precedence);
    try(out != nullptr);
    break;
  }
  }

  return out;
}

struct FieldsAndBodyResult {
  bool ok;
  ArrayList<Node *> fields;
  ArrayList<Node *> body;
};

FieldsAndBodyResult parseFieldsAndBody(ASTParser *parser) {
  ArrayList<Node *> fields;
  ArrayList<Node *> body;
  fields.init(8);
  body.init(8);

  bool allow_field = true;
  while (parser->cur_token.kind != TokenKind::BlockEnd) {
    Node *field = (Node *)parser->allocator.alloc(sizeof(Node));
    field->token = parser->cur_token;
    field->location = parser->cur_token.location;
    field->kind = NodeKind::Name;
    field->text = parser->cur_token.text;
    if (!parser->nextToken()) {
      return {false};
    }

    field = parseField(parser, field);

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

FieldsAndBodyResult parseMembersAndBody(ASTParser *parser) {
  ArrayList<Node *> fields;
  ArrayList<Node *> body;
  fields.init(8);
  body.init(8);

  bool allow_member = true;
  while (parser->cur_token.kind != TokenKind::BlockEnd) {
    Node *field = (Node *)parser->allocator.alloc(sizeof(Node));
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
        field->member.value = parseExpr(parser, Precedence::Assign);
      }

      fields.push(field);

      allow_member = parser->cur_token.kind == TokenKind::CommaDelimiter;
      if (allow_member && !parser->nextToken()) {
        return {false};
      }

      continue;
    }

    field = parseField(parser, field);
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

Node *parseExpr(ASTParser *parser, Precedence min_precedence) {
  Node *out = (Node *)parser->allocator.alloc(sizeof(Node));
  out->token = parser->cur_token;
  out->location = parser->cur_token.location;

  switch (parser->cur_token.kind) {
  case TokenKind::Operator: {
    // Parse Unary
    out->kind = NodeKind::UnaryOperator;
    out->unary_operator.opcode = (UnaryOperator)parser->cur_token._operator;

    try(parser->nextToken());
    out->unary_operator.child = parseExpr(parser, Precedence::MemberAccess);
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
    out->function.parameters.init(8);
    out->function.return_type = nullptr;
    out->function.body = nullptr;

    try(parser->nextToken());

    // Parse Parameters
    try(parser->cur_token.kind == TokenKind::ScopeBegin);
    try(parser->nextToken());
    while (parser->cur_token.kind != TokenKind::ScopeEnd) {
      Node *parameter = (Node *)parser->allocator.alloc(sizeof(Node));
      parameter->token = parser->cur_token;
      parameter->location = parser->cur_token.location;
      parameter->kind = NodeKind::Name;
      parameter->text = parser->cur_token.text;
      try(parser->nextToken());

      parameter = parseField(parser, parameter);
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
        parser->cur_token.kind != TokenKind::LineDelimiter) {
      out->function.return_type = parseExpr(parser, Precedence::Assign);
      try(out->function.return_type != nullptr);
    }

    // Parse Body
    if (parser->cur_token.kind == TokenKind::BlockBegin) {
      out->function.body = parseStmtCompound(parser);
      try(out->function.body != nullptr);
    }

    break;
  }
  case TokenKind::Struct: {
    out->kind = NodeKind::Struct;
    try(parser->nextToken());
    try(parser->cur_token.kind == TokenKind::BlockBegin);
    try(parser->nextToken());

    FieldsAndBodyResult result = parseFieldsAndBody(parser);
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
      out->_enum.repr_type = parseExpr(parser, Precedence::Assign);
    }

    try(parser->cur_token.kind == TokenKind::BlockBegin);
    try(parser->nextToken());

    FieldsAndBodyResult result = parseMembersAndBody(parser);
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
      out->_union.repr_type = parseExpr(parser, Precedence::Assign);
    }

    try(parser->cur_token.kind == TokenKind::BlockBegin);
    try(parser->nextToken());

    FieldsAndBodyResult result = parseFieldsAndBody(parser);
    try(result.ok);
    out->_union.variants = result.fields;
    out->_union.body = result.body;

    try(parser->cur_token.kind == TokenKind::BlockEnd);
    parser->nextToken();
    break;
  }
  }

  out = parsePartialExpr(parser, min_precedence, out);
  try(out != nullptr);

  return out;
}

// The `name_prealloc` argument must contain the name of the variable, and is
// used as a preallocated node for the output
Node *parseField(ASTParser *parser, Node *name_prealloc) {
  Node *out = name_prealloc;
  try(out->kind == NodeKind::Name);

  out->kind = NodeKind::Field;
  out->field.name = out->text;
  out->field.definition = false;
  out->field.type = nullptr;
  out->field.initial = nullptr;

  // Parse Type
  try(parser->cur_token.kind == TokenKind::TypeSeperator);
  try(parser->nextToken());
  if (parser->cur_token.kind != TokenKind::TypeSeperator &&
      (parser->cur_token.kind != TokenKind::Operator ||
       parser->cur_token._operator != Operator::Assign)) {
    out->field.type =
        parseExpr(parser, (Precedence)((int32_t)Precedence::Assign + 1));
    try(out->field.type);
  }

  // Parse Initial
  out->field.definition = parser->cur_token.kind == TokenKind::TypeSeperator;
  if (out->field.definition ||
      (parser->cur_token.kind == TokenKind::Operator &&
       parser->cur_token._operator == Operator::Assign)) {
    try(parser->nextToken());
    out->field.initial = parseExpr(parser, Precedence::Assign);
    try(out->field.initial);
  }

  return out;
}

Node *parseConditional(ASTParser *parser) {
  Node *out = (Node *)parser->allocator.alloc(sizeof(Node));
  out->token = parser->cur_token;
  out->location = parser->cur_token.location;
  out->kind = NodeKind::Compound;
  out->children.init(2);

  while (parser->cur_token.kind != TokenKind::BlockBegin) {
    Node *child;

    if (parser->cur_token.kind == TokenKind::Name) {
      child = (Node *)parser->allocator.alloc(sizeof(Node));
      child->token = parser->cur_token;
      child->location = parser->cur_token.location;
      child->kind = NodeKind::Name;
      child->text = parser->cur_token.text;

      try(parser->nextToken());
      if (parser->cur_token.kind == TokenKind::TypeSeperator) {
        child = parseField(parser, child);
      } else {
        child = parsePartialExpr(
            parser, (Precedence)((int32_t)Precedence::Assign + 1), child);
      }
    } else {
      child = parseExpr(parser, Precedence::Assign);
    }

    out->children.push(child);

    if (parser->cur_token.kind != TokenKind::LineDelimiter) {
      break;
    }
    try(parser->nextToken());
  }

  return out;
}

Node *parseStmt(ASTParser *parser) {
  Node *out = nullptr;

  switch (parser->cur_token.kind) {
  case TokenKind::Name: {
    out = (Node *)parser->allocator.alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = out->token.location;
    out->kind = NodeKind::Name;
    out->text = out->token.text;

    try(parser->nextToken());
    if (parser->cur_token.kind == TokenKind::TypeSeperator) {
      out = parseField(parser, out);
    } else {
      out = parsePartialExpr(parser, Precedence::Assign, out);
    }
    break;
  }
  case TokenKind::Operator: {
    out = parseExpr(parser, Precedence::Assign);
    break;
  }
  case TokenKind::Return: {
    out = (Node *)parser->allocator.alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Return;
    out->child = nullptr;

    try(parser->nextToken());
    if (parser->cur_token.kind != TokenKind::LineDelimiter) {
      out->child = parseExpr(parser, Precedence::Assign);
    }
    break;
  }
  case TokenKind::If: {
    out = (Node *)parser->allocator.alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::If;

    // Parse Conditional
    try(parser->nextToken());
    out->_if.conditional = parseConditional(parser);
    // out->_if.conditional = parseExpr(parser, Precedence::Assign);
    try(parser->cur_token.kind == TokenKind::BlockBegin);

    // Parse Body
    out->_if.body = parseStmtCompound(parser);
    out->_if._else = nullptr;

    // Parse Else and Else-If
    if (parser->cur_token.kind == TokenKind::Else) {
      try(parser->nextToken());

      if (parser->cur_token.kind == TokenKind::If) {
        out->_if._else = parseStmt(parser);
      } else if (parser->cur_token.kind == TokenKind::BlockBegin) {
        out->_if._else = parseStmtCompound(parser);
      } else {
        std::cerr << "Else body must be either an if statement or compound\n";
        return nullptr;
      }
    }

    break;
  }
  }

  try(out);
  if (parser->cur_token.kind == TokenKind::LineDelimiter) {
    parser->nextToken();
  } else if (parser->prev_token.kind != TokenKind::BlockEnd) {
    std::cerr << "Statement must end with either `;` or `}`\n";
    return nullptr;
  }

  return out;
}

Node *parseStmtCompound(ASTParser *parser) {
  Node *out = (Node *)parser->allocator.alloc(sizeof(Node));
  out->kind = NodeKind::Compound;
  out->token = parser->cur_token;
  out->location = parser->cur_token.location;
  out->children.init(8);

  // Pass Marker
  if (parser->cur_token.kind == TokenKind::BlockBegin) {
    try(parser->nextToken());
  }

  // Parse Compound
  while (true) {
    Node *child = parseStmt(parser);
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

  this->ast = parseStmtCompound(this);
}

bool ASTParser::nextToken() {
  Token next_token = this->tokenizer.next();
  while (next_token.kind == TokenKind::Comment) {
    next_token = this->tokenizer.next();
  }

  this->prev_token = this->cur_token;
  this->cur_token = next_token;
  return this->cur_token.kind != TokenKind::Eof;
}
