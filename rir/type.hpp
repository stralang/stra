#pragma once

#include "allocator.hpp"
#include "arenalist.hpp"
#include "containers.hpp"
#include <cstdint>

struct RIRTypeContext; // Forward Declaration

using RIRTypeId = uint32_t;

enum class RIRTypeKind : uint8_t {
  Void,
  Bool,
  Integer,
  Float,
  Pointer,
  Slice,
  Function,
  Struct,
  Enum,
  Union,
  TypeId,
};

struct RIRType {
  uint64_t hashcode;
  RIRTypeKind kind;
  union {
    struct {
      bool is_untyped;
      bool is_signed;
      int32_t bits; // negative is pointer size
    } integer;
    struct {
      bool is_untyped;
      uint32_t bits;
    } _float;
    RIRTypeId child;
    struct {
      int64_t length;
      RIRTypeId child;
    } slice;
    struct {
      Slice<RIRTypeId> arguments;
      RIRTypeId _return;
    } function;
    struct {
      Slice<RIRTypeId> fields;
      uint64_t unique;
    } _struct;
    struct {
      RIRTypeId repr;
      uint64_t unique;
    } _enum;
    struct {
      RIRTypeId repr;
      Slice<RIRTypeId> variants;
      uint64_t unique;
    } _union;
  };

  size_t sizeBits(RIRTypeContext *ctx, size_t native_size);
  size_t alignBits(RIRTypeContext *ctx, size_t native_size);

  void makeHashcode();
};

struct RIRTypeContext {
private:
  ArenaList<RIRType> list;
  HashMap<uint64_t, RIRTypeId> hash_mapping;

public:
  void init(Allocator *general_allocator, Allocator *arena_allocator) {
    this->list.init(general_allocator, arena_allocator, sizeof(RIRType) * 256);
    this->hash_mapping.init(general_allocator, 256);
  }

  void deinit() {
    this->list.deinit();
    this->hash_mapping.deinit();
  }

  size_t len() { return this->list.len(); }

  RIRTypeId push(RIRType type) {
    type.makeHashcode();
    RIRTypeId *original = this->hash_mapping.get(type.hashcode);
    if (original != nullptr) {
      return *original;
    }

    RIRTypeId id = this->list.len();
    this->hash_mapping.insert(type.hashcode, id);
    this->list.push(type);
    return id;
  }

  RIRType *getPtr(RIRTypeId id) { return this->list.getPtr(id); }

  RIRType *getPtrUnchecked(RIRTypeId id) {
    return this->list.getPtrUnchecked(id);
  }
};
