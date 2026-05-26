#pragma once

#include "allocator.hpp"
#include "ast.hpp"
#include "containers.hpp"
#include "token.hpp"
#include <cstddef>
#include <cstring>
#include <sstream>
#include <string>

struct Symbol {
  bool location_aware;
  Symbol *parent;
  Node *node;

  ArrayList<Symbol *> children;

  String mangled_name;

  Symbol *findSymbol(String *name, SrcLoc *location) {
    for (size_t i = 0; i < this->children.length; i++) {
      Symbol *child = this->children.data.ptr[i];
      String *child_name = &child->node->field.name;
      if (child_name->len != name->len ||
          memcmp(child_name->ptr, name->ptr, name->len) != 0) {
        continue;
      }

      if (this->location_aware && location != nullptr &&
          location->index < child->node->location.index) {
        continue;
      }

      return child;
    }

    if (this->parent != nullptr) {
      return this->parent->findSymbol(name, location);
    }
    return nullptr;
  }

  Symbol *findSymbolByNode(Node *node) {
    for (size_t i = 0; i < this->children.length; i++) {
      Symbol *child = this->children.data.ptr[i];
      if (child->node != node) {
        continue;
      }

      return child;
    }

    if (this->parent != nullptr) {
      return this->parent->findSymbolByNode(node);
    } else if (this->node == node) {
      return this;
    }
    return nullptr;
  }

  String mangleName(Allocator *allocator) {
    if (this->mangled_name.ptr != nullptr) {
      return this->mangled_name;
    }

    if (node->kind == NodeKind::Field && node->field.name.compare("main")) {
      this->mangled_name = node->field.name;
      return this->mangled_name;
    }

    String s = {.len = 0, .ptr = nullptr};
    if (this->parent != nullptr) {
      s = this->parent->mangleName(allocator);
    }

    this->mangled_name = s;
    if (node->kind == NodeKind::Field) {
      std::string len_str = std::to_string(node->field.name.len);
      char *text = (char *)allocator->alloc(s.len + len_str.size() +
                                            node->field.name.len);
      if (s.ptr != nullptr) {
        memcpy(text, s.ptr, s.len);
      }

      memcpy(text + s.len, len_str.data(), len_str.size());
      memcpy(text + s.len + len_str.size(), node->field.name.ptr,
             node->field.name.len);

      s.ptr = (uint8_t *)text;
      s.len = s.len + len_str.size() + node->field.name.len;
    }

    this->mangled_name = s;
    return this->mangled_name;
  }

  void init(Allocator *allocator, bool location_aware, Symbol *parent) {
    this->location_aware = location_aware;
    this->parent = parent;
    this->node = nullptr;
    this->children.init(allocator, 8);
    this->mangled_name = {.len = 0, .ptr = nullptr};

    parent->children.push(this);
  }
};
