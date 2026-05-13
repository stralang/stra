#pragma once

#include "allocator.hpp"
#include "arraylist.hpp"
#include "token.hpp"
#include "tokenizer.hpp"
#include "types.hpp"
#include <cstdint>

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

struct NodeFunction {
  ArrayList<Node *> parameters;
  Node *return_type;
  Node *body;
};

struct NodeStruct {
  ArrayList<Node *> fields;
  ArrayList<Node *> body;
};

struct NodeEnum {
  Node *repr_type;
  ArrayList<Node *> members;
  ArrayList<Node *> body;
};

struct NodeUnion {
  Node *repr_type;
  ArrayList<Node *> variants;
  ArrayList<Node *> body;
};

struct NodeMember {
  String name;
  Node *value;
};

struct NodeIf {
  Node *conditional;
  Node *body;
  Node *_else;
};

struct NodeFor {
  Node *conditional;
  Node *body;
};

enum class NodeKind {
  Compound,
  Name,
  Integer,
  Float,
  Char,
  String,
  Field,
  Function,
  Struct,
  Enum,
  Union,
  Member,
  UnaryOperator,
  Operator,
  Return,
  If,
  For,
};

struct Node {
  Token token;
  SrcLoc location;
  NodeKind kind;
  union {
    ArrayList<Node *> children;
    Node *child;
    String text;
    int64_t integer;
    double _float;
    NodeField field;
    NodeFunction function;
    NodeStruct _struct;
    NodeEnum _enum;
    NodeUnion _union;
    NodeMember member;
    NodeUnaryOperator unary_operator;
    NodeOperator _operator;
    NodeIf _if;
    NodeFor _for;
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
