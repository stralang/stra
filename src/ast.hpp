#pragma once

#include "allocator.hpp"
#include "token.hpp"
#include "tokenizer.hpp"
#include "types.hpp"

struct Node;

struct NodeUnaryOperator {
  UnaryOperator opcode;
  Node *child;
};

struct NodeOperator {
  Operator opcode;
  Node *lhs;
  Node *rhs;
};

enum class NodeKind {
  Name,
  UnaryOperator,
  Operator,
};

struct Node {
  Token token;
  SrcLoc location;
  NodeKind kind;
  union {
    String text;
    NodeUnaryOperator unary_operator;
    NodeOperator _operator;
  };
};

struct ASTParser {
  Tokenizer tokenizer;
  Node *ast;

  Token prev_token;
  Token cur_token;

  Allocator allocator;

  void parse();
  bool nextToken();
};
