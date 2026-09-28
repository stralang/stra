#pragma once

#include "containers.hpp"
#include "srcloc.hpp"
#include <sstream>

std::ostream &operator<<(std::ostream &os, const String &str);
std::ostream &operator<<(std::ostream &os, const SrcLoc &location);
