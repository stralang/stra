#include "environment.hpp"

String readTTField(const char **raw) {
  String out = {.len = 0, .ptr = (uint8_t *)*raw};
  while (**raw != '\0') {
    if (**raw == '-') {
      *raw += 1;
      break;
    }

    *raw += 1;
    out.len += 1;
  }
  return out;
}

TargetTriple decodeTargetTriple(const char *raw) {
  TargetTriple out;

  // Read arch
  String arch = readTTField(&raw);
  if (arch.compare("x86_64")) {
    out.arch = Arch::x86_64;
  }

  // Read vendor
  out.vendor = readTTField(&raw);

  // Read os
  String os = readTTField(&raw);
  if (os.compare("linux")) {
    out.os = Os::Linux;
  }

  // Read subos
  String subos = readTTField(&raw);

  return out;
}
