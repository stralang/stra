#pragma once

#include "allocator.hpp"
#include "ast.hpp"
#include "containers.hpp"
#include <cstddef>
#include <cstring>

struct Symbol {
  bool location_aware;
  Symbol *parent;
  Node *node;
  String *name;

  ArrayList<Symbol *> children;

  Symbol *getSymbolInScope(String *name, SrcLoc *location) {
    for (size_t i = 0; i < this->children.length; i++) {
      Symbol *child = this->children.data.ptr[i];
      if (child->node == nullptr) {
        continue;
      }

      // Check location
      if (this->location_aware && location != nullptr &&
          location->index < child->node->location.index) {
        continue;
      }

      // Get name
      String *child_name = child->name;
      if (child_name == nullptr) {
        continue;
      }

      // Compare name
      if (child_name->len != name->len ||
          memcmp(child_name->ptr, name->ptr, name->len) != 0) {
        continue;
      }

      return child;
    }

    return nullptr;
  }

  Symbol *findSymbol(String *name, SrcLoc *location) {
    Symbol *found = this->getSymbolInScope(name, location);
    if (found != nullptr) {
      return found;
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

  // Find duplicate
  bool findDuplicateField(String *name, Node *original, size_t depth = 0) {
    bool is_record = this->node->kind == NodeKind::Struct ||
                     this->node->kind == NodeKind::Enum ||
                     this->node->kind == NodeKind::Union;

    for (size_t i = 0; i < this->children.length; i++) {
      Symbol *child = this->children.data.ptr[i];
      if (child->node == original) {
        continue;
      }

      // Check location
      if (original->location.index < child->node->location.index) {
        // Prevent first field with name thinking it's a duplicate
        continue;
      }

      // Ignore "data" fields
      if (depth > 0 && is_record) {
        if (this->node->kind == NodeKind::Enum &&
            child->node->kind == NodeKind::Member) {
          continue;
        } else if (child->node->kind == NodeKind::Field &&
                   !child->node->field.definition) {
          continue;
        }
      }

      // Get name
      String *child_name = child->name;
      if (child_name == nullptr) {
        continue;
      }

      // Compare name
      if (child_name->len == name->len &&
          memcmp(child_name->ptr, name->ptr, name->len) == 0) {
        return true;
      }
    }

    if (this->parent == nullptr) {
      return false;
    }

    if (depth == 0 && is_record) {
      // Ignore parent of record for "data" fields
      if (this->node->kind == NodeKind::Enum &&
          original->kind == NodeKind::Member) {
        return false;
      } else if (original->kind == NodeKind::Field &&
                 !original->field.definition) {
        return false;
      }
    }

    return this->parent->findDuplicateField(name, original, depth + 1);
  }

  void init(Allocator *allocator, bool location_aware, Symbol *parent) {
    this->location_aware = location_aware;
    this->parent = parent;
    this->node = nullptr;
    this->children.init(allocator, 8);
    this->name = nullptr;

    parent->children.push(this);
  }
};

void symbolize(Allocator *allocator, Node *node, Symbol *parent);
