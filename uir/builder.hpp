#pragma once

#include "uir.hpp"

struct UIRBuilder {
  UIRBlock *block;
  UIRScope *scope;
  UIRModule *module;

  UIRBlock *appendBlock(UIRValue *function, String name);

  UIRValue *insert(UIRValue inst, bool global = false,
                   String name = {.ptr = nullptr});

  UIRValue *buildLocalVariable(UIRValue *type, String name);
  UIRValue *buildLoad(UIRValue *ptr, String name = {.ptr = nullptr});
  UIRValue *buildStore(UIRValue *value, UIRValue *ptr);

  UIRValue *buildArg(UIRValue *type, String name);

  UIRValue *buildBinOp(UIRValue *lhs, UIRValue *rhs, UIROpcode opcode,
                       String name = {.ptr = nullptr});
  UIRValue *buildUnaryOp(UIRValue *value, UIROpcode opcode,
                         String name = {.ptr = nullptr});

  UIRValue *buildCall(UIRValue *callee, Slice<UIRValue *> arguments,
                      Option<UIRValue *> receiver,
                      String name = {.ptr = nullptr});

  UIRValue *buildIndex(UIRValue *ptr, UIRValue *index,
                       String name = {.ptr = nullptr});
  UIRValue *buildRange(UIRValue *ptr, UIRValue *start, UIRValue *end,
                       String name = {.ptr = nullptr});
  UIRValue *buildLookupPtr(UIRValue *ptr, String member,
                           String name = {.ptr = nullptr});
  UIRValue *buildLookupValue(UIRValue *ptr, String member,
                             String name = {.ptr = nullptr});
  UIRValue *buildAggregate(UIRValue *type, Slice<String> names,
                           Slice<UIRValue *> values,
                           String name = {.ptr = nullptr});

  // If `value` is null then this returns `void`
  UIRValue *buildReturn(Option<UIRValue *> value);

  UIRValue *buildBr(UIRBlock *block);
  UIRValue *buildCondBr(UIRValue *condition, UIRBlock *then, UIRBlock *_else);

  UIRValue *buildSwitch(UIRValue *value, UIRBlock *default_block, size_t cases);
  void addCase(UIRValue *switch_inst, UIRValue *onval, UIRBlock *then);

  UIRValue *buildComptime(String name);
  UIRValue *buildTypeOf(UIRValue *value, String name = {.ptr = nullptr});

  UIRValue *buildGlobalVariable(Option<UIRValue *> type,
                                Option<UIRValue *> constant, String name);
  UIRValue *buildFunction(Slice<UIRValue *> parameters, UIRValue *return_type,
                          String name);

  UIRValue *buildLiteral(UIRLiteral literal, String name = {.ptr = nullptr});

  UIRValue *buildPointer(UIRValue *child_type, String name);
  UIRValue *buildSlice(UIRValue *element, UIRValue *length, bool is_pointer,
                       String name);
  UIRValue *buildStruct(Slice<UIRStruct::Field> fields, String name);
  UIRValue *buildEnum(UIRValue *repr_type, Slice<UIREnum::Member> members,
                      String name);
  UIRValue *buildUnion(UIRValue *repr_type, Slice<UIRStruct::Field> variants,
                       String name);
  UIRValue *buildNamespace(String name);
};
