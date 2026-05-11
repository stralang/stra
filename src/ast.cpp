#include "ast.hpp"
#include "operator.hpp"
#include "token.hpp"
#include <cassert>

#define try(is_eof)                                                            \
  if (is_eof) {                                                                \
    return nullptr;                                                            \
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
    break;
  }
  case TokenKind::Name: {
    out->kind = NodeKind::Name;
    out->text = parser->cur_token.text;
    parser->nextToken();
    break;
  }
  }

  return out;
}

void ASTParser::parse() {
  if (this->nextToken() == false) {
    this->ast = nullptr;
    return;
  }

  this->ast = parseExpr(this, Precedence::Assign);
}

bool ASTParser::nextToken() {
  this->prev_token = this->cur_token;
  this->cur_token = this->tokenizer.next();
  return this->cur_token.kind != TokenKind::Eof;
}
