#include "helper.hpp"

bool containsAttribute(Node *attributes, String name) {
  for (size_t i = 0; i < attributes->children.length; i++) {
    Node *child = attributes->children.data.ptr[i];
    if (child->member.name.compare(name)) {
      return true;
    }
  }

  return false;
}

Node *getAttribute(Node *attributes, String name) {
  for (size_t i = 0; i < attributes->children.length; i++) {
    Node *child = attributes->children.data.ptr[i];
    if (child->member.name.compare(name)) {
      return child->member.value;
    }
  }

  return nullptr;
}
