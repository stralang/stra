#pragma once

#include "allocator.hpp"
#include "ast.hpp"
#include "containers.hpp"
#include "token.hpp"
#include <cstddef>
#include <cstring>
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
      if (child->node == nullptr) {
        continue;
      }

      // Check location
      if (this->location_aware && location != nullptr &&
          location->index < child->node->location.index) {
        continue;
      }

      // Get name
      String *child_name;
      if (child->node->kind == NodeKind::Field) {
        child_name = &child->node->field.name;
      } else if (child->node->kind == NodeKind::Member) {
        child_name = &child->node->member.name;
      } else if (child->node->kind == NodeKind::In) {
        child_name = &child->node->in.name;
      } else {
        continue;
      }

      // Compare name
      if (child_name->len != name->len ||
          memcmp(child_name->ptr, name->ptr, name->len) != 0) {
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

  // Find duplicate
  bool findDuplicateField(String *name, Node *original, size_t depth = 0) {
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

      // Get name
      String *child_name;
      if (child->node->kind == NodeKind::Field) {
        child_name = &child->node->field.name;

        if (depth != 0 && !child->node->field.definition) {
          continue;
        }
      } else if (child->node->kind == NodeKind::Member) {
        child_name = &child->node->member.name;
      } else if (child->node->kind == NodeKind::In) {
        child_name = &child->node->in.name;
      } else {
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

    if (depth == 0) {
      // Ignore parent of record for "data" fields
      if ((this->node->kind == NodeKind::Struct ||
           this->node->kind == NodeKind::Union) &&
          original->kind == NodeKind::Field && !original->field.definition) {
        return false;
      } else if (this->node->kind == NodeKind::Enum &&
                 original->kind == NodeKind::Member) {
        return false;
      }
    }

    return this->parent->findDuplicateField(name, original, depth + 1);
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
