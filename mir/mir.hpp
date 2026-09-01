#pragma once

#include "allocator.hpp"
#include "containers.hpp"
#include "literal.hpp"
#include "srcloc.hpp"
#include "types.hpp"
#include <cassert>
#include <cstdint>

// Forward declarations
struct MIRValue;
struct MIRBlock;
struct MIRScope;
// Forward declarations

enum class MIRValueKind : std::uint16_t {
  Nop,

  Instruction = 0x1000,
  Alloca,
  Load,
  Store,
  Arg,
  BinOp,
  UnaryOp,
  Call,
  GEP,
  Lookup,
  Return,
  Branch,
  CondBranch,
  Switch,

  Comptime = 0x2000,
  TypeOf,

  Global = 0x3000,
  GlobalVariable,
  Function,

  Constant = 0x4000,
  Literal,
  Slice,
  Struct,
  Enum,
  Union,
  Namespace,
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

struct MIRInlineComptime {
  ArrayList<MIRBlock *> blocks;

  MIRBlock *appendBlock(String name);
};

struct MIRFunction {
  Slice<MIRValue *> parameter_types;
  MIRValue *return_type;
  MIRScope *globals;
  ArrayList<MIRBlock *> blocks;
  bool undefined;

  MIRBlock *appendBlock(String name);
};

struct MIRSlice {
  MIRValue *element;
  MIRValue *length;
  bool is_pointer;
};

struct MIRStruct {
  struct Field {
    String name;
    MIRValue *type;
  };

  Slice<Field> fields;
  MIRScope *definitions;
};

struct MIREnum {
  struct Member {
    String name;
    MIRValue *constant;
  };

  MIRValue *repr_type;
  Slice<Member> members;
  MIRScope *definitions;
};
struct MIRUnion {
  MIRValue *repr_type;
  Slice<MIRStruct::Field> variants;
  MIRScope *definitions;
};

struct MIRNamespace {
  MIRScope *definitions;
};

struct MIRValue {
  size_t id;
  String name;
  SrcLoc source_location;

  MIRValueKind kind = MIRValueKind::Nop;
  MIRBlock *parent = nullptr;
  Type *result_type = nullptr;

  union {
    struct {
      MIRValue *type;
    } alloca;
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
    } lookup;
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
      MIRBlock *default_block;
      Slice<MIRValue *> onvals;
      Slice<MIRBlock *> blocks;
      size_t slots;
    } _switch;
    // TODO: Comptime
    struct {
      // TODO: Assembly in MIR
    } assembly;

    MIRInlineComptime comptime;
    MIRValue *_typeof;

    struct {
      MIRValue *type;
      MIRValue *constant; // set to `null` for default
      bool undefined;
    } global_variable;
    MIRFunction function;

    MIRLiteral literal;
    MIRStruct _struct;
    MIREnum _enum;
    MIRUnion _union;
    MIRNamespace _namespace;
    MIRSlice slice;
  };
};

struct MIRBlock {
  size_t id;
  String name;
  MIRValue *parent;
  ArrayList<MIRValue *> instructions;

  bool hasTerminator();
};

struct MIRScope {
  MIRValue *owner = nullptr;
  ArrayList<MIRValue *> list;
};

struct MIRContext {
  TypeCache *type_cache;
  DynamicArena arena;
  Allocator *allocator;

  void init(Allocator *allocator);
  void deinit();

  MIRValue *make(MIRValue value);
  MIRValue *makeLiteral(MIRLiteral literal);
};

struct MIRModule {
  size_t next_id = 0;
  MIRScope *definitions;

  DynamicArena arena; // Memory that stores `MIRValue`
  MIRContext *ctx;

  void init(Allocator *allocator);
  void deinit();
};

struct MIRBuilder {
  MIRBlock *block;
  MIRScope *scope;
  MIRModule *module;

  MIRBlock *appendBlock(MIRValue *function, String name);

  MIRValue *insert(MIRValue inst, bool global = false,
                   String name = {.ptr = nullptr});

  MIRValue *buildAlloca(MIRValue *type, String name);
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
  MIRValue *buildLookup(MIRValue *ptr, String member,
                        String name = {.ptr = nullptr});

  // If `value` is null then this returns `void`
  MIRValue *buildReturn(MIRValue *value);

  MIRValue *buildBr(MIRBlock *block);
  MIRValue *buildCondBr(MIRValue *condition, MIRBlock *then, MIRBlock *_else);

  MIRValue *buildSwitch(MIRValue *value, MIRBlock *default_block, size_t cases);
  void addCase(MIRValue *switch_inst, MIRValue *onval, MIRBlock *then);

  MIRValue *buildComptime(String name);
  MIRValue *buildTypeOf(MIRValue *value, String name);

  MIRValue *buildGlobalVariable(MIRValue *type, MIRValue *constant,
                                String name);
  MIRValue *buildFunction(Slice<MIRValue *> parameters, MIRValue *return_type,
                          String name);

  MIRValue *buildStruct(Slice<MIRStruct::Field> fields, String name);
  MIRValue *buildEnum(MIRValue *repr_type, Slice<MIREnum::Member> members,
                      String name);
  MIRValue *buildUnion(MIRValue *repr_type, Slice<MIRStruct::Field> variants,
                       String name);
  MIRValue *buildNamespace(String name);
  MIRValue *buildSlice(MIRValue *element, MIRValue *length, bool is_pointer,
                       String name);
};
