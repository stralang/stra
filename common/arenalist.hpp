#pragma once

#include "allocator.hpp"
#include <cstring>
#include <iostream>

template <typename T> struct ArenaList {
private:
  T **arenas;
  size_t arena_size;
  size_t count;
  size_t length;
  Allocator *record_allocator;
  Allocator *arena_allocator;

public:
  // `record_allocator` used to allocate the pointer store containing each arena
  // `arena_allocator` used to allocate the data
  void init(Allocator *record_allocator, Allocator *arena_allocator,
            size_t arena_size) {
    this->record_allocator = record_allocator;
    this->arena_allocator = arena_allocator;
    this->arena_size = arena_size;
    this->length = 0;

    this->count = 1;
    this->arenas = (T **)record_allocator->alloc(sizeof(T *) * 1);
    this->arenas[0] = (T *)arena_allocator->alloc(sizeof(T) * this->arena_size);
  }
  void deinit() {
    for (size_t i = 0; i < this->count; i++) {
      arena_allocator->_free((uint8_t *)this->arenas[i]);
    }
    record_allocator->_free((uint8_t *)this->arenas);
    this->count = 0;
    this->length = 0;
  }

  inline size_t len() { return this->length; }

  void push(T value) {
    if (this->length == this->count * this->arena_size) {
      grow();
    }

    size_t arena_idx = this->length / this->arena_size;
    size_t data_idx = this->length % this->arena_size;

    this->arenas[arena_idx][data_idx] = value;
    this->length += 1;
  }

  T *getPtrUnchecked(size_t index) {
    size_t arena_idx = index / this->arena_size;
    size_t data_idx = index % this->arena_size;
    return this->arenas[arena_idx] + data_idx;
  }

  inline T getUnchecked(size_t index) { return *this->getPtrUnchecked(index); }

  T *getPtr(size_t index) {
    if (this->length <= index) {
      std::cerr << "Index out of bounds. " << index << " >=" << this->length
                << "\n";
      std::abort();
    }

    return this->getPtrUnchecked(index);
  }

  inline T get(size_t index) { return *this->getPtr(index); }

  T *back() {
    if (this->length == 0) {
      std::cerr << "Cannot index back of empty ArenaList\n";
      std::abort();
    }

    return this->getPtrUnchecked(this->length - 1);
  }

private:
  void grow() {
    size_t new_count = this->count + 1;
    T **new_arenas = (T **)record_allocator->alloc(sizeof(T *) * new_count);
    memcpy(new_arenas, this->arenas, sizeof(T *) * this->count);

    new_arenas[this->count] =
        (T *)arena_allocator->alloc(sizeof(T) * this->arena_size);

    this->record_allocator->_free((uint8_t *)this->arenas);
    this->arenas = new_arenas;
    this->count = new_count;
  }
};
