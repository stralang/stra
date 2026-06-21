#include "helper.hpp"

Node *getAttribute(Node *attributes, String name) {
  for (size_t i = 0; i < attributes->children.length; i++) {
    Node *child = attributes->children.data.ptr[i];
    if (child->member.name.compare(name)) {
      return child;
    }
  }

  return nullptr;
}
