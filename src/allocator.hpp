#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>

struct Allocator {
  uint8_t *alloc(size_t size) { return (uint8_t *)malloc(size); }
  uint8_t *_realloc(uint8_t *ptr, size_t size) {
    return (uint8_t *)realloc(ptr, size);
  }
  void _free(uint8_t *ptr) { free((void *)ptr); }
};
