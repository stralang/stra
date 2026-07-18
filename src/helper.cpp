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

std::string replaceAll(std::string haystack, std::string needle,
                       std::string to) {
  if (needle.empty()) {
    return haystack;
  }

  size_t pos = 0;
  while (true) {
    pos = haystack.find(needle);
    if (pos == std::string::npos) {
      break;
    }

    haystack.replace(pos, needle.length(), to);
    pos += to.length();
  }

  return haystack;
}
