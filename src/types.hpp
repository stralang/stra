#pragma once

#include "containers.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

struct Type;
struct Symbol;

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
  Symbol *scope;
};

struct StructType {
  ArrayList<Type *> fields;
  Symbol *scope;
};

struct EnumType {
  Type *repr_type;
  Symbol *scope;
};

struct UnionType {
  Type *repr_type;
  ArrayList<Type *> variants;
  Symbol *scope;
};

struct Namespace {
  Symbol *scope;
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
  Union,
  Namespace,
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
    Namespace _namespace;
  };
  bool is_constant;
  uint64_t hashcode;

  size_t sizeBits(size_t native_size) {
    switch (this->kind) {
    case TypeKind::Void: {
      return 0;
    }
    case TypeKind::Bool: {
      return 1;
    }
    case TypeKind::Integer: {
      return this->integer.bits == -1 ? native_size : this->integer.bits;
    }
    case TypeKind::Float: {
      return this->_float.bits;
    }
    case TypeKind::Pointer: {
      return this->child->sizeBits(native_size);
    }
    case TypeKind::Slice: {
      size_t elem_size = this->slice.type->sizeBits(native_size);
      return this->slice.length > 0 ? (elem_size * this->slice.length)
                                    : (elem_size + native_size);
    }
    case TypeKind::SIMD: {
      return this->slice.type->sizeBits(native_size) * this->slice.length;
    }
    case TypeKind::TypeId: {
      return 0;
    }
    case TypeKind::Function: {
      return native_size;
    }
    case TypeKind::Struct: {
      size_t total_size = 0;
      size_t max_align = 0;
      for (size_t i = 0; i < this->_struct.fields.length; i++) {
        size_t elem_size =
            this->_struct.fields.data.ptr[i]->sizeBits(native_size);
        size_t elem_align =
            this->_struct.fields.data.ptr[i]->alignBits(native_size);
        size_t padding = elem_align - (total_size % elem_align);

        total_size += padding + elem_size;
        max_align = std::max(max_align, elem_align);
      }

      return total_size + max_align - (total_size % max_align);
    }
    case TypeKind::Enum: {
      return this->_enum.repr_type->sizeBits(native_size);
    }
    case TypeKind::Union: {
      size_t max_size = 0;
      for (size_t i = 0; i < this->_union.variants.length; i++) {
        max_size = std::max(
            max_size, this->_union.variants.data.ptr[i]->sizeBits(native_size));
      }
      return max_size + this->_union.repr_type->sizeBits(native_size);
    }
    case TypeKind::Namespace: {
      break;
    }
    }

    return 0;
  }

  size_t alignBits(size_t native_size) {
    switch (this->kind) {
    case TypeKind::Void: {
      return 0;
    }
    case TypeKind::Bool: {
      return 1;
    }
    case TypeKind::Integer: {
      return this->integer.bits == -1 ? native_size : this->integer.bits;
    }
    case TypeKind::Float: {
      return this->_float.bits;
    }
    case TypeKind::Pointer: {
      return native_size;
    }
    case TypeKind::Slice: {
      return this->slice.type->alignBits(native_size);
    }
    case TypeKind::SIMD: {
      return this->slice.type->alignBits(native_size) * this->slice.length;
    }
    case TypeKind::TypeId: {
      return 0;
    }
    case TypeKind::Function: {
      return native_size;
    }
    case TypeKind::Struct: {
      size_t max_align = 0;
      for (size_t i = 0; i < this->_struct.fields.length; i++) {
        max_align =
            std::max(max_align,
                     this->_struct.fields.data.ptr[i]->alignBits(native_size));
      }
      return max_align;
    }
    case TypeKind::Enum: {
      return this->_enum.repr_type->alignBits(native_size);
    }
    case TypeKind::Union: {
      size_t max_align = this->_union.repr_type->alignBits(native_size);
      for (size_t i = 0; i < this->_union.variants.length; i++) {
        max_align =
            std::max(max_align,
                     this->_union.variants.data.ptr[i]->alignBits(native_size));
      }
      return max_align;
    }
    case TypeKind::Namespace: {
      break;
    }
    }

    return 0;
  }
};

struct TypeCache {
  DynamicArena arena;
  size_t length;

  void init(Allocator *allocator) {
    this->arena.init(allocator, sizeof(Type) * 256);
    this->length = 0;
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
    this->length += 1;
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
    case TypeKind::Namespace: {
      hasher.hash(&t->_namespace.scope);
      break;
    }
    }

    return hasher.state;
  }
};
