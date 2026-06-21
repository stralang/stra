#pragma once

#include "ast.hpp"
#include <cstring>

Node *getAttribute(Node *attributes, String name);
inline Node *getAttribute(Node *attributes, const char *name) {
  return getAttribute(attributes,
                      String{.len = strlen(name), .ptr = (uint8_t *)name});
}
inline bool containsAttribute(Node *attributes, String name) {
  return getAttribute(attributes, name) != nullptr;
}
inline bool containsAttribute(Node *attributes, const char *name) {
  return containsAttribute(attributes,
                           String{.len = strlen(name), .ptr = (uint8_t *)name});
}
