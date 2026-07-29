#include "helper.hpp"
#include <filesystem>
#include <string>

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

std::string makeAbsolute(std::string path, std::string importer,
                         HashMap<String, String> *packages) {
  size_t package_idx = path.find(':');

  std::filesystem::path root_path;
  std::filesystem::path path_cpp;
  if (package_idx == std::string::npos) {
    root_path = importer;
    path_cpp = path;
  } else {
    // Package
    std::string package = path.substr(0, package_idx);
    String *opt_root = packages->get(
        {.len = package.length(), .ptr = (uint8_t *)package.data()});
    if (opt_root == nullptr) {
      return nullptr;
    }

    std::string root((const char *)opt_root->ptr, opt_root->len);
    root_path = root;
    path_cpp = path.substr(package_idx + 1);
  }

  return std::filesystem::canonical(root_path / path_cpp);
}

std::string makeRelative(std::string path, std::string importer,
                         HashMap<String, String> *packages) {
  for (size_t i = 0; i < packages->slot_capacity; i++) {
    auto *slot = packages->slots + i;
    if (!slot->alive) {
      continue;
    }

    if (path.compare(0, slot->value.len, (const char *)slot->value.ptr) == 0) {
      std::string key((const char *)slot->key.ptr, slot->key.len);
      std::string value((const char *)slot->value.ptr, slot->value.len);
      std::string s = std::filesystem::relative(path, value);
      return key + ":" + s;
    }
  }

  return std::filesystem::relative(path, importer);
}
