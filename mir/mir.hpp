#pragma once

#include "containers.hpp"
#include <cstdint>

// FIXME: Replace these with the actual types
struct Type;
struct Value {};

// Forward declarations
struct MIRBlock;
// Forward declarations

enum class MIRValueKind : uint8_t {
  Nop,
  Type,
  Literal,
  Field,
  Load,
  Store,
  BinOp,
  UnaryOp,
  GEP,
  Initializer,
  Return,
  Branch,
  CondBranch,
  Switch,
};

enum class MIROpcode : uint8_t {
  Nop,
  Add,
  Sub,
  Mul,
  Div,
  Or,
  Xor,
  And,
  LeftShift,
  RightShift,

  // Comparison
  EqualTo,
  NotEqualTo,
  LessThen,
  GreaterThen,
  LessThenOrEqualTo,
  GreaterThenOrEqualTo,

  // Cast
  As,
  Bitcast,

  // Unary
  LogicalNot,
  BitwiseNot,
};

struct MIRValue {
  MIRValueKind kind = MIRValueKind::Nop;
  Type *type = nullptr; // Resulting type

  union {
    Value literal;
    struct {
      MIRValue *type;
      MIRValue *initial;
    } field;
    struct {
      MIRValue *ptr;
    } load;
    struct {
      MIRValue *value;
      MIRValue *ptr;
    } store;
    struct {
      ArrayList<MIRBlock *> blocks;
    } function;
    struct {
      MIROpcode opcode;
      MIRValue *lhs;
      MIRValue *rhs;
    } binop;
    struct {
      MIROpcode opcode;
      MIRValue *value;
    } unaryop;
    struct {
      MIRValue *callee;
      ArrayList<MIRValue *> arguments;
    } call;
    struct {
      MIRValue *ptr;
      MIRValue *index;
    } gep;
    struct {
      MIRValue *type;
      ArrayList<MIRValue *> values;
    } initalizer;
    struct {
      MIRValue *value;
    } ret;
    MIRBlock *br;
    struct {
      MIRValue *condition;
      MIRBlock *then;
      MIRBlock *_else;
    } condbr;
    struct {
      MIRValue *condition;
      ArrayList<MIRValue *> cases;
      ArrayList<MIRBlock *> blocks;
    } _switch;
    // TODO: Comptime
    struct {
      // TODO: Assembly in MIR
    } assembly;
  };
};

struct MIRBlock {
  ArrayList<MIRValue *> instructions;
};

struct MIRBuilder {
  MIRValue *function;
  MIRBlock *block;
};
