#include "debug.hpp"

std::ostream &operator<<(std::ostream &os, const String &str) {
  return os.write((const char *)str.ptr, str.len);
}

std::ostream &operator<<(std::ostream &os, const SrcLoc &location) {
  return os << "[`" << location.file << "` " << location.line << ":"
            << location.column << "]";
}
