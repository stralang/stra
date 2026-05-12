#pragma once

#include "allocator.hpp"
#include "arraylist.hpp"
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

struct NodeField {
  String name;
  Node *type;
  Node *initial;
  bool definition;
};

enum class NodeKind {
  Name,
  UnaryOperator,
  Operator,
  Field,
  Compound,
};

struct Node {
  Token token;
  SrcLoc location;
  NodeKind kind;
  union {
    String text;
    NodeUnaryOperator unary_operator;
    NodeOperator _operator;
    NodeField field;
    ArrayList<Node *> children;
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
