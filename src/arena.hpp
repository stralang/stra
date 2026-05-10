#pragma once

#include <cstddef>
#include <cstdlib>

template <typename T> struct Arena {
  T *ptr;
  size_t capacity = 256;
  size_t length = 0;

  void init() { this->ptr = (T *)malloc(sizeof(T) * this->capacity); }
  void deinit() { free(this->ptr); }

  size_t alloc() {
    if (this->length == this->capacity) {
      this->capacity *= 2;
      this->ptr = realloc(this->ptr, sizeof(T) * this->capacity);
    }

    this->length += 1;
    return this->length - 1;
  }
};
