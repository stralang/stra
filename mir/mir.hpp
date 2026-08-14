#pragma once

#include "allocator.hpp"
#include "containers.hpp"
#include "literal.hpp"
#include "types.hpp"
#include <cassert>
#include <cstdint>

// Forward declarations
struct MIRValue;
struct MIRBlock;
// Forward declarations

enum class MIRValueKind : std::uint16_t {
  Nop,

  Instruction = 0x1000,
  Field,
  Load,
  Store,
  Arg,
  BinOp,
  UnaryOp,
  Call,
  GEP,
  MemberAccess,
  Return,
  Branch,
  CondBranch,
  Switch,

  Constant = 0x2000,
  Literal,
  Function,
};

enum class MIROpcode : uint8_t {
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

struct MIRFunction {
  Slice<MIRValue *> parameter_types;
  MIRValue *return_type;
  ArrayList<MIRBlock> blocks;
};

struct MIRValue {
  size_t id;
  String name;
  MIRValueKind kind = MIRValueKind::Nop;
  Type *result_type = nullptr;

  union {
    struct {
      MIRValue *type;
      MIRValue *initial; // a `null` initial is externally defined
    } field;
    struct {
      MIRValue *ptr;
    } load;
    struct {
      MIRValue *value;
      MIRValue *ptr;
    } store;
    struct {
      MIRValue *type;
    } arg;
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
      MIRValue *receiver; // NOTE: this is only valid after type checking
    } call;
    struct {
      MIRValue *ptr;
      MIRValue *index;
    } gep;
    struct {
      MIRValue *parent;
      String member;
    } member_access;
    struct {
      MIRValue *type;
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

    MIRLiteral literal;
    MIRFunction function;
  };
};

struct MIRBlock {
  MIRFunction *function;
  ArrayList<MIRValue> instructions;
};

struct MIRContext {
  TypeCache *type_cache;
  ArrayList<MIRValue> values;
  Allocator *allocator;

  void init(Allocator *allocator);
  void deinit();

  MIRValue *make(MIRValue value);
  MIRValue *makeLiteral(MIRLiteral literal);
};

struct MIRModule {
  size_t next_id = 0;
  ArrayList<MIRValue> instructions;

  Allocator *allocator;
  MIRContext *ctx;

  void init(Allocator *allocator);
  void deinit();
};

struct MIRBuilder {
  MIRBlock *block;
  MIRModule *module;

  MIRValue *insert(MIRValue inst, bool global = false,
                   String name = {.ptr = nullptr});

  MIRValue *buildField(MIRValue *type, MIRValue *initial, String name);
  MIRValue *buildLoad(MIRValue *ptr, String name = {.ptr = nullptr});
  MIRValue *buildStore(MIRValue *value, MIRValue *ptr);

  MIRValue *buildArg(MIRValue *type, String name);

  MIRValue *buildBinOp(MIRValue *lhs, MIRValue *rhs, MIROpcode opcode,
                       String name = {.ptr = nullptr});
  MIRValue *buildUnaryOp(MIRValue *value, MIROpcode opcode,
                         String name = {.ptr = nullptr});

  MIRValue *buildCall(MIRValue *callee, Slice<MIRValue *> arguments,
                      MIRValue *receiver, String name = {.ptr = nullptr});

  MIRValue *buildGEP(MIRValue *ptr, MIRValue *index,
                     String name = {.ptr = nullptr});
  MIRValue *buildMemberAccess(MIRValue *ptr, String member,
                              String name = {.ptr = nullptr});

  // If `value` is null then this returns `void`
  MIRValue *buildReturn(MIRValue *value);

  MIRValue *buildBr(MIRBlock *block);
  MIRValue *buildCondBr(MIRValue *condition, MIRBlock *then, MIRBlock *_else);

  MIRValue *buildSwitch(MIRValue *value, size_t cases);
  void addCase(MIRValue *switch_inst, MIRValue *onval, MIRBlock *then);
};
