#pragma once

#include "allocator.hpp"
#include "containers.hpp"
#include "token.hpp"
#include "tokenizer.hpp"
#include "types.hpp"
#include <cstdint>

struct Node;
struct Symbol;

struct NodeField {
  String name;
  Node *type;
  Node *initial;

  Node *attributes;
  bool definition;
  bool undefined;
};

struct NodeFunction {
  ArrayList<Node *> parameters;
  Node *return_type;
  Node *body;
  bool undefined;
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

struct NodeImport {
  String path;
  Node *node;
  Symbol *scope;
};

struct NodeSlice {
  bool is_pointer;
  Node *length;
  Node *type;
};

struct NodeUnaryOperator {
  UnaryOperator opcode;
  Node *child;
};

struct NodeOperator {
  Operator opcode;
  Node *lhs;
  Node *rhs;
};

struct NodeCall {
  Node *callee;
  ArrayList<Node *> arguments;
};

struct NodeIndex {
  Node *slice;
  Node *index;
};

struct NodeInitializer {
  Node *record;
  ArrayList<Node *> setters;
  bool is_list;
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

struct NodeSwitch {
  Node *conditional;
  ArrayList<Node *> cases;
};

struct NodeCase {
  Node *constant;
  Node *body;
};

struct NodeAssembly {
  struct Argument {
    Token token;
    SrcLoc location;
    enum { Input, Return, Register } kind;
    union {
      String reg;
      Node *node;
    };
  };

  struct Instruction {
    Token token;
    SrcLoc location;
    String name;
    ArrayList<Argument> arguments;
  };

  ArrayList<Instruction> instructions;
};

struct Value {
  Type *type;
  bool has_data;
  union {
    Type *type_value;
    String text;
    bool _bool;
    int64_t integer;
    double _float;
    Node *node;
  } data;
};

enum class NodeKind {
  Compound,
  Name,
  Bool,
  Integer,
  Float,
  Char,
  String,
  Field,
  Function,
  Struct,
  Enum,
  Union,
  Namespace,
  Member,
  Import,
  Const,
  Slice,
  UnaryOperator,
  Operator,
  Call,
  Index,
  Initializer,
  Return,
  If,
  For,
  Switch,
  Case,
  Break,
  Continue,
  Defer,
  Comptime,
  Assembly,

  Attribute,
  CommentGroup,
};

struct Node {
  Token token;
  SrcLoc location;
  SrcLoc end_location;
  Value value;

  Node *doc_comments;
  Node *line_comments;

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
    NodeImport import;
    NodeSlice slice;
    NodeUnaryOperator unary_operator;
    NodeOperator _operator;
    NodeCall call;
    NodeIndex index;
    NodeInitializer initializer;
    NodeIf _if;
    NodeFor _for;
    NodeSwitch _switch;
    NodeCase _case;
    NodeAssembly assembly;
    ArrayList<Token> comment_group;
  };
};
