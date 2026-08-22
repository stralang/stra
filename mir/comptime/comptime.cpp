#include "comptime.hpp"
#include "literal.hpp"
#include "mir.hpp"
#include <cstdlib>
#include <iostream>

MIRLiteral MIRComptime::execute(MIRModule *module, MIRValue *inst) {
  std::cerr << "TODO: Implement compile-time execution\n";
  std::abort();
}
