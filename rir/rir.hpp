#pragma once

#include "allocator.hpp"
#include "arenalist.hpp"
#include "containers.hpp"
#include "optional.hpp"
#include "srcloc.hpp"
#include <cstddef>
#include <cstdint>

struct RIRType;
struct RIRBlock;

enum class RIRValueKind : std::uint16_t {
  Nop,

  LocalVariable,
  Load,
  Store,
  Arg,
  BinOp,
  UnaryOp,
  Call,
  GEP,
  Return,
  Branch,
  CondBranch,
  Switch,

  GlobalVariable,
  Function,
  Struct,
  Enum,
  Union,

  Literal,
  Pointer,
  Slice,
};

enum class RIROpcode : uint8_t {
  Nop,
  Add,
  Sub,
  Mul,
  Div,
  Mod,
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
  Minus,
  LogicalNot,
  BitwiseNot,
};

struct RIRValue {
  size_t id;
  String name;
  SrcLoc source_location;

  RIRType *result_type;
  RIRValueKind kind;
  union {
    struct {
      RIRType *type;
    } local;
    struct {
      RIRValue *ptr;
    } load;
    struct {
      RIRValue *ptr;
      RIRValue *value;
    } store;
    struct {
      RIRType *type;
    } arg;
    struct {
      RIROpcode opcode;
      RIRValue *lhs;
      RIRValue *rhs;
    } binop;
    struct {
      RIROpcode opcode;
      RIRValue *value;
    } unaryop;
    struct {
      RIRValue *callee;
      Slice<RIRValue *> arguments;
    } call;
    struct {
      RIRValue *ptr;
      RIRValue *index;
    } gep;
    struct {
      Option<RIRValue *> value;
    } ret;
    RIRBlock *branch;
    struct {
      RIRValue *condition;
      RIRBlock *then;
      RIRBlock *_else;
    } cond_branch;
    struct {
      RIRValue *condition;
      RIRBlock *default_block;
      Slice<RIRValue *> onvals;
      Slice<RIRBlock *> blocks;
      size_t slots;
    } _switch;
    struct {
      // TODO: Assembly in RIR
    } assembly;

    struct {
      RIRType *type;
      Option<RIRValue *> constant; // null for undefined
    } global_variable;
    struct {
      Slice<RIRType *> parameter_types;
      RIRType *return_type;
      ArrayList<RIRBlock *> blocks;
      bool undefined;
    } function;
    struct {
      Slice<RIRType *> fields;
    } _struct;
    struct {
      RIRType *tag_type;
      Slice<RIRType *> variants;
    } _union;
  };
};

struct RIRBlock {
  size_t id;
  String name;
  RIRValue *function;
  ArrayList<RIRValue *> instructions;
};

struct RIRModule {
  size_t id;
  ArenaList<RIRValue> instructions;
  ArenaList<RIRBlock> blocks;

  ArrayList<RIRValue *> roots;

  Allocator *allocator;

  void init(Allocator *allocator, Allocator *arena_allocator);
  void deinit();
};
