#pragma once

#include "allocator.hpp"
#include "arenalist.hpp"
#include "containers.hpp"
#include "optional.hpp"
#include "srcloc.hpp"
#include "type.hpp"
#include <cstddef>
#include <cstdint>

struct RIRBlock; // Forward Declaration

struct RIRValueId {
  uint32_t module;
  uint32_t local;
};
struct RIRBlockId {
  uint32_t module;
  uint32_t local;
};

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

  Constant,
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
  RIRValueId id;

  RIRTypeId result_type;
  RIRValueKind kind;
  union {
    struct {
      RIRTypeId type;
    } local;
    struct {
      RIRValueId ptr;
    } load;
    struct {
      RIRValueId ptr;
      RIRValueId value;
    } store;
    struct {
      RIRTypeId type;
    } arg;
    struct {
      RIROpcode opcode;
      RIRValueId lhs;
      RIRValueId rhs;
    } binop;
    struct {
      RIROpcode opcode;
      RIRValueId value;
    } unaryop;
    struct {
      RIRValueId callee;
      Slice<RIRValueId> arguments;
    } call;
    struct {
      RIRValueId ptr;
      RIRValueId index;
    } gep;
    struct {
      Option<RIRValueId> value;
    } ret;
    RIRBlockId branch;
    struct {
      RIRValueId condition;
      RIRBlockId then;
      RIRBlockId _else;
    } cond_branch;
    struct {
      RIRValueId condition;
      RIRBlockId default_block;
      Slice<RIRValueId> onvals;
      Slice<RIRBlockId> blocks;
      size_t slots;
    } _switch;
    struct {
      // TODO: Assembly in RIR
    } assembly;

    struct {
      RIRTypeId type;
      Option<RIRValueId> constant; // null for undefined
    } global_variable;
    struct {
      RIRTypeId type;
      ArrayList<RIRBlockId> blocks;
      bool undefined;
    } function;

    struct {
      RIRTypeId type;
      // TODO: constant data
    } constant;
  };
};

struct RIRBlock {
  RIRBlockId id;
  RIRValueId function;
  ArrayList<RIRValueId> instructions;
};

struct RIRModule {
  uint32_t id;
  ArenaList<RIRValue> instructions;
  ArenaList<RIRBlock> blocks;

  ArrayList<RIRValueId> roots;

  Allocator *allocator;

  void init(Allocator *allocator, Allocator *arena_allocator);
  void deinit();
};

struct RIRContext {
  ArenaList<RIRType> types;
  ArenaList<RIRModule> modules;

  Allocator *allocator;

  void init(Allocator *allocator, Allocator *arena_allocator);
  void deinit();

  RIRValue *getInst(RIRValueId id);
  RIRBlock *getBlock(RIRBlockId id);
  RIRType *getType(RIRTypeId id);
};
