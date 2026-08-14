#include "containers.hpp"
#include <string>

String str(const char *s) {
  return String{.ptr = (uint8_t *)s, .len = strlen(s)};
}

String str(std::string s) {
  return String{.ptr = (uint8_t *)s.data(), .len = s.size()};
}
