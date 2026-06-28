#pragma once

#include "allocator.hpp"
#include "ast.hpp"
#include <cstring>

Node *getAttribute(Node *attributes, String name);
inline Node *getAttribute(Node *attributes, const char *name) {
  return getAttribute(attributes,
                      String{.ptr = (uint8_t *)name, .len = strlen(name)});
}
inline bool containsAttribute(Node *attributes, String name) {
  return getAttribute(attributes, name) != nullptr;
}
inline bool containsAttribute(Node *attributes, const char *name) {
  return containsAttribute(attributes,
                           String{.ptr = (uint8_t *)name, .len = strlen(name)});
}

Node *astCopy(Allocator *allocator, Node *src, Symbol *scope);

std::string replaceAll(std::string haystack, std::string needle,
                       std::string to);

std::string makeAbsolute(std::string path, std::string importer,
                         HashMap<String, String> *packages);
std::string makeRelative(std::string path, std::string importer,
                         HashMap<String, String> *packages);
