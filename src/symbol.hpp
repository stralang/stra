#pragma once

#include "allocator.hpp"
#include "ast.hpp"
#include "containers.hpp"
#include "token.hpp"
#include <cstddef>
#include <cstring>

struct Scope;

struct Symbol {
  Scope *parent;
  Node *node;
};

struct Scope {
  bool location_aware;
  Scope *parent;
  ArrayList<Scope *> children;
  ArrayList<Symbol *> symbols;
  Node *node;

  Symbol *findSymbol(String *name, SrcLoc *location) {
    for (size_t i = 0; i < this->symbols.length; i++) {
      Symbol *child = this->symbols.data.ptr[i];
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

  Scope *findScope(Node *node) {

    for (size_t i = 0; i < this->children.length; i++) {
      Scope *child = this->children.data.ptr[i];
      if (child->node != node) {
        continue;
      }

      return child;
    }

    if (this->parent != nullptr) {
      return this->parent->findScope(node);
    } else if (this->node == node) {
      return this;
    }
    return nullptr;
  }

  void init(Allocator *allocator, bool location_aware, Scope *parent) {
    this->location_aware = location_aware;
    this->parent = parent;
    this->children.init(allocator, 8);
    this->symbols.init(allocator, 8);

    parent->children.push(this);
  }
};
