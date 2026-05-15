#pragma once

#include "allocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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
    return Slice<T>{.len = end - start + 1, .ptr = this->ptr + start};
  }
};

using String = Slice<uint8_t>;

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

  T pop() {
    this->length -= 1;
    return this->data.ptr[this->length];
  }

  Slice<T> slice() { return this->data.range(0, this->length); }
};

template <typename K, typename V> struct HashMap {
  struct Slot {
    bool tombstone;
    bool alive;
    uint64_t hashcode;
    K key;
    V value;
  };

  Allocator *allocator;
  Slot *slots;
  size_t slot_capacity;
  size_t slot_count;

  void init(Allocator *allocator, size_t slot_capacity) {
    this->allocator = allocator;
    this->slot_capacity = slot_capacity;
    this->slot_count = 0;
    this->slots = (Slot *)allocator->alloc(sizeof(Slot) * this->slot_capacity);
    memset(this->slots, 0, sizeof(Slot) * this->slot_capacity);
  }
  void deinit() { allocator->_free(this->slots); }

  void insert(K key, V value) {
    uint64_t hashcode = hash(key);
    Slot *slot = getSlot(hashcode, false);
    slot->key = key;
    slot->value = value;
  }

  V *get(K key) {
    uint64_t hashcode = hash(key);
    Slot *slot = getSlot(hashcode, true);
    if (slot == nullptr) {
      return nullptr;
    }

    return slot->value;
  }

  void remove(K key) {
    uint64_t hashcode = hash(key);
    Slot *slot = getSlot(hashcode, true);
    if (slot == nullptr) {
      return;
    }

    slot->alive = false;
    slot->tombstone = true;
  }

  uint64_t hash(K k) {
    size_t len = sizeof(K);
    uint8_t *bytes = (uint8_t *)&k;

    uint64_t hash = 0xcbf29ce484222325;
    for (size_t i = 0; i < len; i++) {
      hash = hash ^ bytes[i];
      hash = hash * 0x100000001b3;
    }

    return hash;
  }

  Slot *getSlot(uint64_t hashcode, bool get) {
    size_t slot_idx = hashcode % this->slot_capacity;
    Slot *slot = this->slots + slot_idx;

    for (size_t i = 0; i < this->slot_capacity; i++) {
      if ((!slot->alive && (get && !slot->tombstone)) ||
          slot->hashcode == hashcode) {
        break;
      }
      slot_idx += 1;
      slot = this->slots + slot_idx % this->slot_capacity;
    }

    if (!slot->alive) {
      if (get) {
        return nullptr;
      }

      slot->alive = true;
      slot->tombstone = false;
      slot->hashcode = hashcode;
    }

    return slot;
  }

  void resize(size_t new_capacity) {
    Slot *new_slots = (Slot *)allocator->alloc(sizeof(Slot) * new_capacity);
    memset(new_slots, 0, sizeof(Slot) * new_capacity);

    for (size_t i = 0; i < this->slot_capacity; i++) {
      Slot *old_slot = this->slots + i;
      size_t new_slot_idx = old_slot->hashcode % new_capacity;
      Slot *new_slot = new_slots + new_slot_idx;
      while (new_slot->alive) {
        new_slot_idx += 1;
        new_slot = new_slots + new_slot_idx;
      }

      new_slot->alive = true;
      new_slot->hashcode = old_slot->hashcode;
      new_slot->key = old_slot->key;
      new_slot->value = old_slot->value;
    }

    allocator->_free(this->slots);
    this->slots = new_slots;
  }
};
