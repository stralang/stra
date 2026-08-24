#pragma once

#include "analysis.hpp"
#include <sstream>

#define expect(ok, srcloc, msg)                                                \
  if (!(ok)) {                                                                 \
    std::ostringstream os;                                                     \
    os << msg;                                                                 \
    std::string cpp_str = os.str();                                            \
    String m = {(uint8_t *)cpp_str.data(), cpp_str.size()};                    \
    analyser->error_func(srcloc, m);                                           \
    analyser->error_count += 1;                                                \
  }

void analyse(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst);
