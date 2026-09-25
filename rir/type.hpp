#pragma once

#include "allocator.hpp"
#include "arenalist.hpp"
#include "containers.hpp"
#include <cstdint>

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

  void makeHashcode() {
    Hasher hasher;
    hasher.hash(&this->kind);
    switch (this->kind) {
    case RIRTypeKind::Integer: {
      hasher.hash(&this->integer);
      break;
    }
    case RIRTypeKind::Float: {
      hasher.hash(&this->_float);
      break;
    }
    case RIRTypeKind::Pointer: {
      hasher.hash(&this->child);
      break;
    }
    case RIRTypeKind::Slice: {
      hasher.hash(&this->slice.length);
      hasher.hash(&this->slice.child);
      break;
    }
    case RIRTypeKind::Function: {
      hasher.hash(&this->function.arguments.len);
      for (size_t i = 0; i < this->function.arguments.len; i++) {
        hasher.hash(this->function.arguments.ptr + i);
      }
      hasher.hash(&this->function._return);
      break;
    }
    case RIRTypeKind::Struct: {
      hasher.hash(&this->_struct.unique);
      break;
    }
    case RIRTypeKind::Enum: {
      hasher.hash(&this->_enum.unique);
      break;
    }
    case RIRTypeKind::Union: {
      hasher.hash(&this->_union.unique);
      break;
    }
    }

    this->hashcode = hasher.state;
  }
};

struct RIRTypeCache {
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
