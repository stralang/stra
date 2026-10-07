#pragma once

#include "allocator.hpp"
#include "arenalist.hpp"
#include "containers.hpp"
#include "literal.hpp"
#include "optional.hpp"
#include "rir/type.hpp"
#include "srcloc.hpp"
#include "types.hpp"
#include <cassert>
#include <cstdint>

// Forward declarations
struct UIRContext;
struct UIRValue;
struct UIRBlock;
struct UIRScope;
// Forward declarations

struct UIRValueId {
  uint32_t module;
  uint32_t local;
};
struct UIRBlockId {
  uint32_t module;
  uint32_t local;
};
struct UIRScopeId {
  uint32_t module;
  uint32_t local;
};

enum class UIRValueKind : std::uint16_t {
  Nop,

  Instruction = 0x1000,
  LocalVariable, // Allocated once per stack frame
  Load,
  Store,
  Arg,
  BinOp,
  UnaryOp,
  Call,
  Index,
  Range,
  LookupPtr,
  LookupValue, // like `LookupPtr` but with an implicit load
  Aggregate,
  Return,
  Branch,
  CondBranch,
  Assembly,
  Switch,

  Comptime = 0x2000,
  TypeOf,

  Global = 0x3000,
  GlobalVariable, // Allocated once per process
  Function,

  Constant = 0x4000,
  Literal,
  Pointer,
  Slice,
  Struct,
  Enum,
  Union,
  Namespace,
};

enum class UIROpcode : uint8_t {
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

struct UIRAssembly {
  struct Operand {
    enum { Input, Return, Register } kind;
    union {
      UIRValueId uir;
      String reg;
    };
  };

  String name;
  Slice<Operand> operands;
};

struct UIRInlineComptime {
  ArrayList<UIRBlockId> blocks;

  UIRBlockId appendBlock(String name);
};

struct UIRFunction {
  Slice<UIRValueId> parameter_types;
  UIRValueId return_type;
  ArrayList<UIRBlockId> blocks;
  bool undefined;

  UIRBlockId appendBlock(String name);
};

struct UIRSlice {
  UIRValueId element;
  Option<UIRValueId> length;
  bool is_pointer;
};

struct UIRStruct {
  struct Field {
    String name;
    UIRValueId type;
  };

  Slice<Field> fields;
  UIRScopeId definitions;
};

struct UIREnum {
  struct Member {
    String name;
    Option<UIRValueId> constant;
  };

  UIRValueId repr_type;
  Slice<Member> members;
  UIRScopeId definitions;
};
struct UIRUnion {
  UIRValueId repr_type;
  Slice<UIRStruct::Field> variants;
  UIRScopeId definitions;
};

struct UIRNamespace {
  UIRScopeId definitions;
};

struct UIRValue {
  UIRValueId id;
  String name;
  SrcLoc source_location;

  UIRValueKind kind = UIRValueKind::Nop;
  Option<UIRValueId> parent = {};

  union {
    struct {
      UIRValueId type;
    } local_variable;
    struct {
      UIRValueId ptr;
    } load;
    struct {
      UIRValueId value;
      UIRValueId ptr;
    } store;
    struct {
      UIRValueId type;
    } arg;
    struct {
      UIROpcode opcode;
      UIRValueId lhs;
      UIRValueId rhs;
    } binop;
    struct {
      UIROpcode opcode;
      UIRValueId value;
    } unaryop;
    struct {
      UIRValueId callee;
      Slice<UIRValueId> arguments;
      Option<UIRValueId>
          receiver; // NOTE: this is only valid after type checking
    } call;
    struct {
      UIRValueId ptr;
      UIRValueId index;
    } index;
    struct {
      UIRValueId ptr;
      UIRValueId start;
      UIRValueId end;
    } range;
    struct {
      UIRValueId parent;
      String member;
    } lookup;
    struct {
      UIRValueId type;
      Slice<String> names; // `len = 0` is a list/unnamed initializer
      Slice<UIRValueId> values;
    } aggregate;
    struct {
      UIRValueId type;
      Option<UIRValueId> value;
    } ret;
    UIRBlockId br;
    struct {
      UIRValueId condition;
      UIRBlockId then;
      UIRBlockId _else;
    } condbr;
    struct {
      UIRValueId condition;
      UIRBlockId default_block;
      Slice<UIRValueId> onvals;
      Slice<UIRBlockId> blocks;
    } _switch;
    Slice<UIRAssembly> assembly;

    UIRInlineComptime comptime;
    UIRValueId _typeof;
    UIRValueId alias;

    struct {
      Option<UIRValueId> type;
      Option<UIRValueId> constant; // set to `null` for default
      bool undefined;
    } global_variable;
    UIRFunction function;

    UIRLiteral literal;
    UIRValueId pointer;
    UIRSlice slice;
    UIRStruct _struct;
    UIREnum _enum;
    UIRUnion _union;
    UIRNamespace _namespace;
  };
};

struct UIRBlock {
  UIRBlockId id;
  String name;
  UIRValueId parent;
  ArrayList<UIRValueId> instructions;

  bool hasTerminator(UIRContext *ctx);
};

struct UIRScope {
  UIRScopeId id;
  Option<UIRValueId> owner = {};
  ArrayList<UIRValueId> list;
};

struct UIRModule {
  uint32_t id;
  UIRScopeId definitions;

  ArenaList<UIRValue> instructions;
  ArenaList<UIRBlock> blocks;
  ArenaList<UIRScope> scopes;

  Allocator *allocator;

  void init(Allocator *allocator, Allocator *arena_allocator);
  void deinit();
};

struct UIRContext {
  RIRTypeContext *types;
  ArenaList<UIRModule> modules;
  Allocator *allocator;

  void init(Allocator *allocator, Allocator *arena_allocator);
  void deinit();

  UIRValue *getInst(UIRValueId id);
  UIRBlock *getBlock(UIRBlockId id);
  UIRScope *getScope(UIRScopeId id);
  RIRType *getType(RIRTypeId id);
};
