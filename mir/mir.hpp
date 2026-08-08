#pragma once

#include "allocator.hpp"
#include "containers.hpp"
#include "literal.hpp"
#include "types.hpp"
#include <cassert>
#include <cstdint>

// Forward declarations
struct MIRBlock;
// Forward declarations

enum class MIRValueKind : uint8_t {
  Nop,
  Literal,
  Field,
  Load,
  Store,
  BinOp,
  UnaryOp,
  Call,
  GEP,
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
    MIRLiteral literal;
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
      Slice<MIRValue *> arguments;
    } call;
    struct {
      MIRValue *ptr;
      MIRValue *index;
    } gep;
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
      Slice<MIRValue *> onvals;
      Slice<MIRBlock *> blocks;
      size_t slots;
    } _switch;
    // TODO: Comptime
    struct {
      // TODO: Assembly in MIR
    } assembly;
  };
};

struct MIRBlock {
  ArrayList<MIRValue> instructions;
};

struct MIRContext {
  ArrayList<MIRValue> values;
  Allocator *allocator;

  void init(Allocator *allocator);
  void deinit();

  MIRValue *make(MIRValue value);
  MIRValue *makeLiteral(MIRLiteral literal);
};

struct MIRModule {
  ArrayList<MIRValue> instructions;

  Allocator *allocator;
  MIRContext *ctx;

  void init(Allocator *allocator);
  void deinit();

  void print();
};

struct MIRBuilder {
  MIRValue *function;
  MIRBlock *block;
  MIRModule *module;

  MIRValue *insert(MIRValue inst, bool global = false);

  MIRValue *buildField(MIRValue *type, MIRValue *initial);
  MIRValue *buildLoad(MIRValue *ptr);
  MIRValue *buildStore(MIRValue *value, MIRValue *ptr);

  MIRValue *buildBinOp(MIRValue *lhs, MIRValue *rhs, MIROpcode opcode);
  MIRValue *buildUnaryOp(MIRValue *value, MIROpcode opcode);

  MIRValue *buildCall(MIRValue *callee, Slice<MIRValue *> arguments);

  MIRValue *buildGEP(MIRValue *ptr, MIRValue *index);

  // If `value` is null then this returns `void`
  MIRValue *buildReturn(MIRValue *value);

  MIRValue *buildBr(MIRBlock *block);
  MIRValue *buildCondBr(MIRValue *condition, MIRBlock *then, MIRBlock *_else);

  MIRValue *buildSwitch(MIRValue *value, size_t cases);
  void addCase(MIRValue *switch_inst, MIRValue *onval, MIRBlock *then);
};
