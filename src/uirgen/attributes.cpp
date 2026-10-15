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
      continue;
    }

    if (attr->member.name.compare("builtin")) {
      if (attr->member.value == nullptr) {
        out.builtin.setStub();
      } else {
        out.builtin.setInline(attr->member.value->text);
      }
      continue;
    }

    if (attr->member.name.compare("implied_comptime")) {
      out.implied_comptime = true;
      continue;
    }
  }

  expect(!out.implied_comptime || !out.builtin.isAbsent(), node->location,
         "Attributes cannot contain `implied_comptime` without `builtin`");

  return out;
}
