#pragma once

#include "mir.hpp"

struct MIRBuilder {
  MIRBlock *block;
  MIRScope *scope;
  MIRModule *module;

  MIRBlock *appendBlock(MIRValue *function, String name);

  MIRValue *insert(MIRValue inst, bool global = false,
                   String name = {.ptr = nullptr});

  MIRValue *buildLocalVariable(MIRValue *type, String name);
  MIRValue *buildLoad(MIRValue *ptr, String name = {.ptr = nullptr});
  MIRValue *buildStore(MIRValue *value, MIRValue *ptr);

  MIRValue *buildArg(MIRValue *type, String name);

  MIRValue *buildBinOp(MIRValue *lhs, MIRValue *rhs, MIROpcode opcode,
                       String name = {.ptr = nullptr});
  MIRValue *buildUnaryOp(MIRValue *value, MIROpcode opcode,
                         String name = {.ptr = nullptr});

  MIRValue *buildCall(MIRValue *callee, Slice<MIRValue *> arguments,
                      Option<MIRValue *> receiver,
                      String name = {.ptr = nullptr});

  MIRValue *buildIndex(MIRValue *ptr, MIRValue *index,
                       String name = {.ptr = nullptr});
  MIRValue *buildRange(MIRValue *ptr, MIRValue *start, MIRValue *end,
                       String name = {.ptr = nullptr});
  MIRValue *buildLookup(MIRValue *ptr, String member,
                        String name = {.ptr = nullptr});

  // If `value` is null then this returns `void`
  MIRValue *buildReturn(Option<MIRValue *> value);

  MIRValue *buildBr(MIRBlock *block);
  MIRValue *buildCondBr(MIRValue *condition, MIRBlock *then, MIRBlock *_else);

  MIRValue *buildSwitch(MIRValue *value, MIRBlock *default_block, size_t cases);
  void addCase(MIRValue *switch_inst, MIRValue *onval, MIRBlock *then);

  MIRValue *buildComptime(String name);
  MIRValue *buildTypeOf(MIRValue *value, String name = {.ptr = nullptr});

  MIRValue *buildGlobalVariable(Option<MIRValue *> type,
                                Option<MIRValue *> constant, String name);
  MIRValue *buildFunction(Slice<MIRValue *> parameters, MIRValue *return_type,
                          String name);

  MIRValue *buildPointer(MIRValue *child_type, String name);
  MIRValue *buildSlice(MIRValue *element, MIRValue *length, bool is_pointer,
                       String name);
  MIRValue *buildStruct(Slice<MIRStruct::Field> fields, String name);
  MIRValue *buildEnum(MIRValue *repr_type, Slice<MIREnum::Member> members,
                      String name);
  MIRValue *buildUnion(MIRValue *repr_type, Slice<MIRStruct::Field> variants,
                       String name);
  MIRValue *buildNamespace(String name);
};
