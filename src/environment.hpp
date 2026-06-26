#pragma once

#include "containers.hpp"
#include <cstdint>

enum class Arch : uint16_t {
  Unknown,
  x86_64,
};

enum class Os : uint16_t {
  Unknown,
  Linux,
};

enum class SubOs : uint16_t {
  Default,
};

enum class Endian : uint8_t {
  Little,
  Big,
};

struct TargetTriple {
  Arch arch = Arch::Unknown;
  String vendor = String{.len = 7, .ptr = (uint8_t *)"unknown"};
  Os os = Os::Unknown;
  SubOs sub_os = SubOs::Default;
};

enum class LibraryScope : uint8_t { Static, Dynamic };

struct Library {
  String name;
  LibraryScope scope;
};

struct Environment {
  TargetTriple target;
  Endian endianness;
  size_t pointer_size;

  ArrayList<Library> link_libraries;
};

TargetTriple decodeTargetTriple(const char *raw);
