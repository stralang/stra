#include "define.hpp"

UIRAttributes genAttributes(UIRGen *uirgen, Node *node, Symbol *scope) {
  UIRAttributes out;
  if (node == nullptr) {
    return out;
  }

  for (size_t a = 0; a < node->children.length; a++) {
    Node *attr = node->children.get(a);

    if (attr->member.name.compare("link_name")) {
      if (attr->member.value == nullptr) {
        out.link_name.setStub();
      } else {
        out.link_name.setInline(attr->member.value->text);
      }
    }
  }

  return out;
}
