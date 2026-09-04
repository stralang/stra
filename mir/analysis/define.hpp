#pragma once

#include "analysis.hpp"
#include "mir.hpp"
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

void analyseBlock(MIRAnalyser *analyser, MIRModule *module, MIRBlock *block);
void analyseScope(MIRAnalyser *analyser, MIRModule *module, MIRScope *scope);

void analyse(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst);
void analyseLookup(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst);
void analyseGlobal(MIRAnalyser *analyser, MIRModule *module, MIRValue *inst);

bool compareTypes(Type *lhs, Type *rhs);
void fixUntyped(MIRAnalyser *analyser, MIRValue *inst, Type *real);
void autoCast(MIRAnalyser *analyser, MIRValue *src, Type *dst);
