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
    return this->float_bits;
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
    return this->float_bits;
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

bool RIRType::compare(RIRTypeContext *ctx, RIRTypeId other_id) {
  RIRType *other = ctx->getPtr(other_id);
  if (this->kind != other->kind) {
    return false;
  }

  switch (this->kind) {
  case RIRTypeKind::Void: {
    return true;
  }
  case RIRTypeKind::Bool: {
    return true;
  }
  case RIRTypeKind::Integer: {
    bool term1 = this->integer.is_signed || !other->integer.is_signed;
    bool term2 = other->integer.is_signed || !this->integer.is_signed;

    bool bits_match = this->integer.bits == other->integer.bits;
    return term1 && term2 && bits_match;
  }
  case RIRTypeKind::Float: {
    return this->float_bits == other->float_bits;
  }
  case RIRTypeKind::Pointer: {
    RIRType *this_child = ctx->getPtr(this->child);
    return this_child->compare(ctx, other->child);
  }
  case RIRTypeKind::Slice: {
    RIRType *this_child = ctx->getPtr(this->slice.child);
    return this->slice.length == other->slice.length &&
           this_child->compare(ctx, other->slice.child);
  }
  case RIRTypeKind::TypeId: {
    return true;
  }
  case RIRTypeKind::Function: {
    if (this->function.arguments.len != other->function.arguments.len) {
      return false;
    }

    for (size_t i = 0; i < this->function.arguments.len; i++) {
      RIRType *this_arg = ctx->getPtr(this->function.arguments.ptr[i]);
      if (!this_arg->compare(ctx, other->function.arguments.ptr[i])) {
        return false;
      }
    }

    RIRType *this_return = ctx->getPtr(this->function._return);
    return this_return->compare(ctx, other->function._return);
  }
  case RIRTypeKind::Struct: {
    return this->_struct.unique == other->_struct.unique;
  }
  case RIRTypeKind::Enum: {
    RIRType *this_repr = ctx->getPtr(this->_enum.repr);
    return this_repr->compare(ctx, other->_enum.repr);
  }
  case RIRTypeKind::Union: {
    return this->_union.unique == other->_union.unique;
  }
  }

  return false;
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
    hasher.hash(&this->float_bits);
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
