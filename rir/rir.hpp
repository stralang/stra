#pragma once

#include "allocator.hpp"
#include "arenalist.hpp"
#include "containers.hpp"
#include "optional.hpp"
#include "rir/constant.hpp"
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
  Cast,
  Call,
  Index,
  Range,
  FieldAt,
  Aggregate,
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

  RIRValueKind kind;
  RIRTypeId result;
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
      RIRValueId value;
      bool bitcast;
    } cast;
    struct {
      RIRValueId callee;
      Slice<RIRValueId> arguments;
    } call;
    struct {
      RIRValueId ptr;
      RIRValueId index;
    } index;
    struct {
      RIRValueId ptr;
      RIRValueId offset;
      RIRValueId length;
    } range;
    struct {
      RIRValueId ptr;
      size_t index;
    } field_at;
    struct {
      Slice<RIRValueId> values;
    } aggregate;
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
    } _switch;
    struct {
      // TODO: Assembly in RIR
    } assembly;

    struct {
      RIRTypeId type;
      Option<RIRConstant> constant; // null for undefined
      String link_name;
    } global_variable;
    struct {
      RIRTypeId type;
      ArrayList<RIRBlockId> blocks;
      bool undefined;
      String link_name;
    } function;

    struct {
      RIRTypeId type;
      RIRConstant value;
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
  RIRTypeContext *types;
  ArenaList<RIRModule> modules;

  Allocator *allocator;

  void init(Allocator *allocator, Allocator *arena_allocator);
  void deinit();

  RIRValue *getInst(RIRValueId id);
  RIRBlock *getBlock(RIRBlockId id);
  RIRType *getType(RIRTypeId id);
};
