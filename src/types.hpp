#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>

template <typename T> struct Slice {
  size_t len;
  T *ptr;

  T &operator[](size_t index) {
    if (index >= this->len) {
      std::cerr << "Index out of bounds" << std::endl;
      std::abort();
    }
    return ptr[index];
  }
  const T &operator[](size_t index) const {
    if (index >= this->len) {
      std::cerr << "Index out of bounds" << std::endl;
      std::abort();
    }
    return ptr[index];
  };

  Slice<T> range(size_t start, size_t end) const {
    return Slice<T>{.len = end - start, .ptr = this->ptr + start};
  }
};

using String = Slice<uint8_t>;
