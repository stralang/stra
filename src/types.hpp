#pragma once

#include "containers.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>

struct Type;
struct Scope;

struct IntegerType {
  bool is_untyped;
  bool is_signed;
  // -1 for native
  int32_t bits;
};

struct FloatType {
  bool is_untyped;
  uint32_t bits;
};

struct SliceType {
  // <0 for pointer slice, =0 for slice, >0 for array (comptile-time length)
  int64_t length;
  Type *type;
};

struct FunctionType {
  ArrayList<Type *> arguments;
  Type *return_type;
  Scope *scope;
};

struct StructType {
  ArrayList<Type *> fields;
  Scope *scope;
};

struct EnumType {
  Type *repr_type;
  Scope *scope;
};

struct UnionType {
  Type *repr_type;
  ArrayList<Type *> variants;
  Scope *scope;
};

enum class TypeKind {
  Void,
  Bool,
  Integer,
  Float,
  Pointer,
  Slice,
  SIMD,
  TypeId,
  Function,
  Struct,
  Enum,
  Union
};

struct Type {
  TypeKind kind;
  union {
    Type *child;
    IntegerType integer;
    FloatType _float;
    SliceType slice;
    FunctionType function;
    StructType _struct;
    EnumType _enum;
    UnionType _union;
  };
  bool is_constant;
  uint64_t hashcode;
};

struct TypeCache {
  DynamicArena arena;
  size_t length;

  void init(Allocator *allocator) {
    this->arena.init(allocator, sizeof(Type) * 256);
  }
  void deinit() { arena.deinit(); }

  Type *get(Type t) {
    uint64_t hashcode = hash(&t);

    for (size_t i = 0; i < length; i++) {
      size_t byte_idx = i * sizeof(Type);
      size_t chunk_idx = byte_idx / arena.min_chunk_size;
      size_t local_idx = byte_idx % arena.min_chunk_size;
      Type *type = (Type *)((arena.chunks + chunk_idx)->ptr + local_idx);
      if (type->hashcode != hashcode) {
        continue;
      }

      return type;
    }

    t.hashcode = hashcode;
    Type *allocation = (Type *)arena.alloc(sizeof(Type));
    *allocation = t;
    return allocation;
  }

  uint64_t hash(Type *t) {
    Hasher hasher;
    hasher.hash(&t->kind);
    hasher.hash(&t->is_constant);

    switch (t->kind) {
    case TypeKind::Void:
    case TypeKind::Bool:
    case TypeKind::TypeId: {
      break;
    }
    case TypeKind::Integer: {
      hasher.hash(&t->integer);
      break;
    }
    case TypeKind::Float: {
      hasher.hash(&t->_float);
      break;
    }
    case TypeKind::Pointer: {
      hasher.hash(&t->child->hashcode);
      break;
    }
    case TypeKind::Slice:
    case TypeKind::SIMD: {
      hasher.hash(&t->slice.type->hashcode);
      hasher.hash(&t->slice.length);
      break;
    }
    case TypeKind::Function: {
      hasher.hash(&t->function.scope);
      break;
    }
    case TypeKind::Struct: {
      hasher.hash(&t->_struct.scope);
      break;
    }
    case TypeKind::Enum: {
      hasher.hash(&t->_enum.scope);
      break;
    }
    case TypeKind::Union: {
      hasher.hash(&t->_union.scope);
      break;
    }
    }

    return hasher.state;
  }
};
