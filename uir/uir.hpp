#pragma once

#include "allocator.hpp"
#include "arenalist.hpp"
#include "containers.hpp"
#include "literal.hpp"
#include "optional.hpp"
#include "srcloc.hpp"
#include "types.hpp"
#include <cassert>
#include <cstdint>

// Forward declarations
struct UIRValue;
struct UIRBlock;
struct UIRScope;
// Forward declarations

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
  Switch,

  Comptime = 0x2000,
  TypeOf,
  Alias,

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

struct UIRInlineComptime {
  ArrayList<UIRBlock *> blocks;

  UIRBlock *appendBlock(String name);
};

struct UIRFunction {
  Slice<UIRValue *> parameter_types;
  UIRValue *return_type;
  UIRScope *globals;
  ArrayList<UIRBlock *> blocks;
  bool undefined;

  UIRBlock *appendBlock(String name);
};

struct UIRSlice {
  UIRValue *element;
  UIRValue *length;
  bool is_pointer;
};

struct UIRStruct {
  struct Field {
    String name;
    UIRValue *type;
  };

  Slice<Field> fields;
  UIRScope *definitions;
};

struct UIREnum {
  struct Member {
    String name;
    UIRValue *constant;
  };

  UIRValue *repr_type;
  Slice<Member> members;
  UIRScope *definitions;
};
struct UIRUnion {
  UIRValue *repr_type;
  Slice<UIRStruct::Field> variants;
  UIRScope *definitions;
};

struct UIRNamespace {
  UIRScope *definitions;
};

struct UIRValue {
  size_t id;
  String name;
  SrcLoc source_location;

  UIRValueKind kind = UIRValueKind::Nop;
  UIRBlock *parent = nullptr;
  Type *result_type = nullptr;

  union {
    struct {
      UIRValue *type;
    } local_variable;
    struct {
      UIRValue *ptr;
    } load;
    struct {
      UIRValue *value;
      UIRValue *ptr;
    } store;
    struct {
      UIRValue *type;
    } arg;
    struct {
      UIROpcode opcode;
      UIRValue *lhs;
      UIRValue *rhs;
    } binop;
    struct {
      UIROpcode opcode;
      UIRValue *value;
    } unaryop;
    struct {
      UIRValue *callee;
      Slice<UIRValue *> arguments;
      Option<UIRValue *>
          receiver; // NOTE: this is only valid after type checking
    } call;
    struct {
      UIRValue *ptr;
      UIRValue *index;
    } index;
    struct {
      UIRValue *ptr;
      UIRValue *start;
      UIRValue *end;
    } range;
    struct {
      UIRValue *parent;
      String member;
    } lookup;
    struct {
      UIRValue *type;
      Slice<String> names; // `len = 0` is a list/unnamed initializer
      Slice<UIRValue *> values;
    } aggregate;
    struct {
      UIRValue *type;
      Option<UIRValue *> value;
    } ret;
    UIRBlock *br;
    struct {
      UIRValue *condition;
      UIRBlock *then;
      UIRBlock *_else;
    } condbr;
    struct {
      UIRValue *condition;
      UIRBlock *default_block;
      Slice<UIRValue *> onvals;
      Slice<UIRBlock *> blocks;
      size_t slots;
    } _switch;
    // TODO: Comptime
    struct {
      // TODO: Assembly in UIR
    } assembly;

    UIRInlineComptime comptime;
    UIRValue *_typeof;
    UIRValue *alias;

    struct {
      Option<UIRValue *> type;
      Option<UIRValue *> constant; // set to `null` for default
      bool undefined;
    } global_variable;
    UIRFunction function;

    UIRLiteral literal;
    UIRValue *pointer;
    UIRSlice slice;
    UIRStruct _struct;
    UIREnum _enum;
    UIRUnion _union;
    UIRNamespace _namespace;
  };
};

struct UIRBlock {
  size_t id;
  String name;
  UIRValue *parent;
  ArrayList<UIRValue *> instructions;

  bool hasTerminator();
};

struct UIRScope {
  UIRValue *owner = nullptr;
  ArrayList<UIRValue *> list;
};

struct UIRContext {
  TypeCache *type_cache;
  DynamicArena arena;
  Allocator *allocator;

  void init(Allocator *allocator);
  void deinit();

  UIRValue *make(UIRValue value);
  UIRValue *makeLiteral(UIRLiteral literal);
};

struct UIRModule {
  size_t next_id = 0;
  UIRScope *definitions;

  ArenaList<UIRValue> instructions;
  ArenaList<UIRBlock> blocks;
  ArenaList<UIRScope> scopes;

  Allocator *allocator;
  UIRContext *ctx;

  void init(Allocator *allocator, Allocator *arena_allocator);
  void deinit();
};
