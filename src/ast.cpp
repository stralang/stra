#include "ast.hpp"
#include "operator.hpp"
#include "print.hpp"
#include "token.hpp"
#include <cassert>
#include <iostream>

#define try(is_ok)                                                             \
  if (!(is_ok)) {                                                              \
    return nullptr;                                                            \
  }

// Forward Declaration
Node *parseExpr(ASTParser *parser, Precedence min_precedence);

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
  }

  out = parsePartialExpr(parser, min_precedence, out);
  try(out != nullptr);

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
      // TODO: Parse Variable
    } else {
      out = parsePartialExpr(parser, Precedence::Assign, out);
    }
    break;
  }
  case TokenKind::Operator: {
    out = parseExpr(parser, Precedence::Assign);
    break;
  }
  }

  if (parser->cur_token.kind != TokenKind::LineDelimiter &&
      parser->prev_token.kind != TokenKind::BlockEnd) {
    std::cerr << "Statement must end with either `;` or `}`\n";
    return nullptr;
  }

  return out;
}

void ASTParser::parse() {
  if (this->nextToken() == false) {
    this->ast = nullptr;
    return;
  }

  this->ast = parseStmt(this);
}

bool ASTParser::nextToken() {
  this->prev_token = this->cur_token;
  this->cur_token = this->tokenizer.next();
  return this->cur_token.kind != TokenKind::Eof;
}
