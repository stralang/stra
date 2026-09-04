#pragma once

#include "allocator.hpp"
#include "containers.hpp"
#include "literal.hpp"
#include "optional.hpp"
#include "srcloc.hpp"
#include "types.hpp"
#include <cassert>
#include <cstdint>

// Forward declarations
struct MIRModule;
// Forward declarations

struct MIRValueId {
  uint32_t module;
  uint32_t local;
};
struct MIRBlockId {
  uint32_t module;
  uint32_t local;
};
struct MIRScopeId {
  uint32_t module;
  uint32_t local;
};

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
  ArrayList<MIRBlockId> blocks;

  MIRBlockId appendBlock(String name);
};

struct MIRFunction {
  Slice<MIRValueId> parameter_types;
  Option<MIRValueId> return_type;
  MIRScopeId globals;
  ArrayList<MIRBlockId> blocks;
  bool undefined;

  MIRBlockId appendBlock(String name);
};

struct MIRSlice {
  MIRValueId element;
  Option<MIRValueId> length;
  bool is_pointer;
};

struct MIRStruct {
  struct Field {
    String name;
    MIRValueId type;
  };

  Slice<Field> fields;
  MIRScopeId definitions;
};

struct MIREnum {
  struct Member {
    String name;
    MIRValueId constant;
  };

  MIRValueId repr_type;
  Slice<Member> members;
  MIRScopeId definitions;
};
struct MIRUnion {
  MIRValueId repr_type;
  Slice<MIRStruct::Field> variants;
  MIRScopeId definitions;
};

struct MIRNamespace {
  MIRScopeId definitions;
};

struct MIRValue {
  MIRValueId id;
  String name;
  SrcLoc source_location;

  MIRBlockId parent;
  MIRValueKind kind = MIRValueKind::Nop;
  Type *result_type = nullptr;

  union {
    struct {
      MIRValueId type;
    } alloca;
    struct {
      MIRValueId ptr;
    } load;
    struct {
      MIRValueId value;
      MIRValueId ptr;
    } store;
    struct {
      MIRValueId type;
    } arg;
    struct {
      MIROpcode opcode;
      MIRValueId lhs;
      MIRValueId rhs;
    } binop;
    struct {
      MIROpcode opcode;
      MIRValueId value;
    } unaryop;
    struct {
      MIRValueId callee;
      Slice<MIRValueId> arguments;
      Option<MIRValueId>
          receiver; // NOTE: this is only valid after type checking
    } call;
    struct {
      MIRValueId ptr;
      MIRValueId index;
    } gep;
    struct {
      MIRValueId parent;
      String member;
    } lookup;
    struct {
      MIRValueId type;
      Option<MIRValueId> value;
    } ret;
    MIRBlockId br;
    struct {
      MIRValueId condition;
      MIRBlockId then;
      MIRBlockId _else;
    } condbr;
    struct {
      MIRValueId condition;
      MIRBlockId default_block;
      Slice<MIRValueId> onvals;
      Slice<MIRBlockId> blocks;
      size_t slots;
    } _switch;
    // TODO: Comptime
    struct {
      // TODO: Assembly in MIR
    } assembly;

    MIRInlineComptime comptime;
    MIRValueId _typeof;

    struct {
      Option<MIRValueId> type;
      Option<MIRValueId> constant;
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
  MIRBlockId id;
  String name;
  MIRValueId parent;
  ArrayList<MIRValueId> instructions;

  bool hasTerminator(MIRModule *module);
};

struct MIRScope {
  MIRScopeId id;
  MIRValueId owner;
  ArrayList<MIRValueId> list;
};

struct MIRContext {
  TypeCache *type_cache;
  DynamicArena arena;
  Allocator *allocator;

  ArrayList<MIRModule *> modules;

  void init(Allocator *allocator);
  void deinit();

  MIRValue *getInstr(MIRValueId id);
  MIRBlock *getBlock(MIRBlockId id);
  MIRScope *getScope(MIRScopeId id);

  MIRValueId make(MIRValue value);
  MIRValueId makeLiteral(MIRLiteral literal);
};

struct MIRModule {
  uint32_t id;
  MIRScopeId definitions;
  ArrayList<MIRValue> instrs;
  ArrayList<MIRBlock> blocks;
  ArrayList<MIRScope> scopes;

  DynamicArena arena; // Memory that stores `MIRValue`
  MIRContext *ctx;

  void init(Allocator *allocator);
  void deinit();

  MIRValue *getInstr(MIRValueId id);
  MIRBlock *getBlock(MIRBlockId id);
  MIRScope *getScope(MIRScopeId id);
};

struct MIRBuilder {
  Option<MIRBlockId> block;
  Option<MIRScopeId> scope;
  MIRModule *module;

  void setSourceLocation(MIRValueId inst, SrcLoc location);

  MIRBlockId appendBlock(MIRValueId function, String name);
  MIRScopeId makeScope(MIRValueId parent);

  MIRValueId insert(MIRValue inst, bool global = false,
                    String name = {.ptr = nullptr});

  MIRValueId buildAlloca(MIRValueId type, String name);
  MIRValueId buildLoad(MIRValueId ptr, String name = {.ptr = nullptr});
  MIRValueId buildStore(MIRValueId value, MIRValueId ptr);

  MIRValueId buildArg(MIRValueId type, String name);

  MIRValueId buildBinOp(MIRValueId lhs, MIRValueId rhs, MIROpcode opcode,
                        String name = {.ptr = nullptr});
  MIRValueId buildUnaryOp(MIRValueId value, MIROpcode opcode,
                          String name = {.ptr = nullptr});

  MIRValueId buildCall(MIRValueId callee, Slice<MIRValueId> arguments,
                       Option<MIRValueId> receiver,
                       String name = {.ptr = nullptr});

  MIRValueId buildGEP(MIRValueId ptr, MIRValueId index,
                      String name = {.ptr = nullptr});
  MIRValueId buildLookup(MIRValueId ptr, String member,
                         String name = {.ptr = nullptr});

  // If `value` is none then this returns `void`
  MIRValueId buildReturn(Option<MIRValueId> value);

  MIRValueId buildBr(MIRBlockId block);
  MIRValueId buildCondBr(MIRValueId condition, MIRBlockId then,
                         MIRBlockId _else);

  MIRValueId buildSwitch(MIRValueId value, MIRBlockId default_block,
                         size_t cases);
  void addCase(MIRValueId _switch, MIRValueId onval, MIRBlockId then);

  MIRValueId buildComptime(String name);
  MIRValueId buildTypeOf(MIRValueId value, String name);

  MIRValueId buildGlobalVariable(Option<MIRValueId> type,
                                 Option<MIRValueId> constant, String name);
  MIRValueId buildFunction(Slice<MIRValueId> parameters,
                           Option<MIRValueId> return_type, String name);

  MIRValueId buildStruct(Slice<MIRStruct::Field> fields, String name);
  MIRValueId buildEnum(MIRValueId repr_type, Slice<MIREnum::Member> members,
                       String name);
  MIRValueId buildUnion(MIRValueId repr_type, Slice<MIRStruct::Field> variants,
                        String name);
  MIRValueId buildNamespace(String name);
  MIRValueId buildSlice(MIRValueId element, Option<MIRValueId> length,
                        bool is_pointer, String name);
};
