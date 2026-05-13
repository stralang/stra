#pragma once

#include "allocator.hpp"
#include "types.hpp"
#include <cstddef>
#include <cstdlib>

template <typename T> struct ArrayList {
  Slice<T> data;
  size_t length;
  Allocator *allocator;

  void init(Allocator *allocator, size_t capacity) {
    this->allocator = allocator;
    this->data.len = capacity;
    this->data.ptr = (T *)allocator->alloc(sizeof(T) * this->data.len);
    this->length = 0;
  }
  void deinit() { allocator->_free(this->data.ptr); }

  void push(T value) {
    if (this->length == this->data.len) {
      this->data.len *= 2;
      this->data.ptr = (T *)allocator->_realloc((uint8_t *)this->data.ptr,
                                                sizeof(T) * this->data.len);
    }

    this->data.ptr[this->length] = value;
    this->length += 1;
  }

  Slice<T> slice() { return this->data.range(0, this->length); }
};
