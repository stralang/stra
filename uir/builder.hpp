#pragma once

#include "containers.hpp"
#include "uir.hpp"

struct UIRBuilder {
  Option<UIRBlock *> block;
  Option<UIRScope *> scope;
  UIRModule *module;
  UIRContext *context;

  UIRScope *createScope();

  UIRBlock *createBlock(String name);
  UIRBlock *appendBlock(UIRValueId function, String name);

  UIRValue *insert(UIRValue inst, bool global = false,
                   String name = {.ptr = nullptr});

  UIRValue *buildLocalVariable(UIRValueId type, String name);
  UIRValue *buildLoad(UIRValueId ptr, String name = {.ptr = nullptr});
  UIRValue *buildStore(UIRValueId value, UIRValueId ptr);

  UIRValue *buildArg(UIRValueId type, String name);

  UIRValue *buildBinOp(UIRValueId lhs, UIRValueId rhs, UIROpcode opcode,
                       String name = {.ptr = nullptr});
  UIRValue *buildUnaryOp(UIRValueId value, UIROpcode opcode,
                         String name = {.ptr = nullptr});

  UIRValue *buildCall(UIRValueId callee, Slice<UIRValueId> arguments,
                      Option<UIRValueId> receiver,
                      String name = {.ptr = nullptr});

  UIRValue *buildIndex(UIRValueId ptr, UIRValueId index,
                       String name = {.ptr = nullptr});
  UIRValue *buildRange(UIRValueId ptr, UIRValueId start, UIRValueId end,
                       String name = {.ptr = nullptr});
  UIRValue *buildLookupPtr(UIRValueId ptr, String member,
                           String name = {.ptr = nullptr});
  UIRValue *buildLookupValue(UIRValueId ptr, String member,
                             String name = {.ptr = nullptr});
  UIRValue *buildAggregate(UIRValueId type, Slice<String> names,
                           Slice<UIRValueId> values,
                           String name = {.ptr = nullptr});

  // If `value` is null then this returns `void`
  UIRValue *buildReturn(Option<UIRValueId> value);

  UIRValue *buildBr(UIRBlockId block);
  UIRValue *buildCondBr(UIRValueId condition, UIRBlockId then,
                        UIRBlockId _else);

  UIRValue *buildSwitch(UIRValueId value, UIRBlockId default_block,
                        Slice<UIRValueId> onvals, Slice<UIRBlockId> blocks);

  UIRValue *buildAssembly(Slice<UIRAssembly> instructions,
                          String name = {.ptr = nullptr});

  UIRValue *buildComptime(String name);
  UIRValue *buildTypeOf(UIRValueId value, String name = {.ptr = nullptr});

  UIRValue *buildGlobalVariable(Option<UIRValueId> type,
                                Option<UIRValueId> constant, String name);
  UIRValue *buildFunction(Slice<UIRValueId> parameters, UIRValueId return_type,
                          String name);

  UIRValue *buildLiteral(UIRLiteral literal, String name = {.ptr = nullptr});

  UIRValue *buildPointer(UIRValueId child_type, String name);
  UIRValue *buildSlice(UIRValueId element, Option<UIRValueId> length,
                       bool is_pointer, String name);
  UIRValue *buildStruct(Slice<UIRStruct::Field> fields, String name);
  UIRValue *buildEnum(UIRValueId repr_type, Slice<UIREnum::Member> members,
                      String name);
  UIRValue *buildUnion(UIRValueId repr_type, Slice<UIRStruct::Field> variants,
                       String name);
  UIRValue *buildNamespace(String name);
};
