#include "type.hpp"

size_t RIRType::sizeBits(RIRTypeContext *ctx, size_t native_size) {
  switch (this->kind) {
  case RIRTypeKind::Void: {
    return 0;
  }
  case RIRTypeKind::Bool: {
    return 1;
  }
  case RIRTypeKind::Integer: {
    return this->integer.bits == -1 ? native_size : this->integer.bits;
  }
  case RIRTypeKind::Float: {
    return this->_float.bits;
  }
  case RIRTypeKind::Pointer: {
    return native_size;
  }
  case RIRTypeKind::Slice: {
    if (this->slice.length > 0) {
      RIRType *elem_type = ctx->getPtr(this->slice.child);
      size_t elem_size = elem_type->sizeBits(ctx, native_size);
      return elem_size * this->slice.length; // Array
    } else if (this->slice.length == 0) {
      return native_size * 2; // Slice
    }
    return native_size; // Pointer Slice
  }
  case RIRTypeKind::TypeId: {
    return 0;
  }
  case RIRTypeKind::Function: {
    return native_size;
  }
  case RIRTypeKind::Struct: {
    size_t total_size = 0;
    size_t max_align = 0;
    for (size_t i = 0; i < this->_struct.fields.len; i++) {
      RIRType *elem_type = ctx->getPtr(this->_struct.fields.ptr[i]);
      size_t elem_size = elem_type->sizeBits(ctx, native_size);
      size_t elem_align = elem_type->alignBits(ctx, native_size);
      size_t padding = -total_size % elem_align;

      total_size += padding + elem_size;
      max_align = std::max(max_align, elem_align);
    }

    return total_size + (-total_size % max_align);
  }
  case RIRTypeKind::Enum: {
    RIRType *repr_type = ctx->getPtr(this->_enum.repr);
    return repr_type->sizeBits(ctx, native_size);
  }
  case RIRTypeKind::Union: {
    size_t max_size = 0;
    for (size_t i = 0; i < this->_union.variants.len; i++) {
      RIRType *elem_type = ctx->getPtr(this->_union.variants.ptr[i]);
      max_size = std::max(max_size, elem_type->sizeBits(ctx, native_size));
    }
    RIRType *repr_type = ctx->getPtr(this->_enum.repr);
    return max_size + repr_type->sizeBits(ctx, native_size);
  }
  }

  return 0;
}

size_t RIRType::alignBits(RIRTypeContext *ctx, size_t native_size) {
  switch (this->kind) {
  case RIRTypeKind::Void: {
    return 0;
  }
  case RIRTypeKind::Bool: {
    return 1;
  }
  case RIRTypeKind::Integer: {
    return this->integer.bits == -1 ? native_size : this->integer.bits;
  }
  case RIRTypeKind::Float: {
    return this->_float.bits;
  }
  case RIRTypeKind::Pointer: {
    return native_size;
  }
  case RIRTypeKind::Slice: {
    if (this->slice.length > 0) {
      RIRType *elem_type = ctx->getPtr(this->slice.child);
      return elem_type->alignBits(ctx, native_size);
    }
    return native_size;
  }
  case RIRTypeKind::TypeId: {
    return 0;
  }
  case RIRTypeKind::Function: {
    return native_size;
  }
  case RIRTypeKind::Struct: {
    size_t max_align = 0;
    for (size_t i = 0; i < this->_struct.fields.len; i++) {
      RIRType *elem_type = ctx->getPtr(this->_struct.fields.ptr[i]);
      max_align = std::max(max_align, elem_type->alignBits(ctx, native_size));
    }
    return max_align;
  }
  case RIRTypeKind::Enum: {
    RIRType *repr_type = ctx->getPtr(this->_enum.repr);
    return repr_type->sizeBits(ctx, native_size);
  }
  case RIRTypeKind::Union: {
    RIRType *repr_type = ctx->getPtr(this->_union.repr);
    size_t max_align = repr_type->alignBits(ctx, native_size);
    for (size_t i = 0; i < this->_union.variants.len; i++) {
      RIRType *elem_type = ctx->getPtr(this->_union.variants.ptr[i]);
      max_align = std::max(max_align, elem_type->alignBits(ctx, native_size));
    }
    return max_align;
  }
  }

  return 0;
}

void RIRType::makeHashcode() {
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
