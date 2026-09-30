#pragma once

#include "../uir.hpp"
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

void analyseBlock(UIRAnalyser *analyser, UIRModule *module, UIRBlock *block);
void analyseScope(UIRAnalyser *analyser, UIRModule *module, UIRScope *scope);

void analyse(UIRAnalyser *analyser, UIRModule *module, UIRValue *inst);
void analyseBinary(UIRAnalyser *analyser, UIRModule *module, UIRValue *inst);
void analyseUnary(UIRAnalyser *analyser, UIRModule *module, UIRValue *inst);
void analyseLookupPtr(UIRAnalyser *analyser, UIRModule *module, UIRValue *inst);
void analyseLookupValue(UIRAnalyser *analyser, UIRModule *module,
                        UIRValue *inst);
void analyseGlobal(UIRAnalyser *analyser, UIRModule *module, UIRValue *inst);
void analyseAggregate(UIRAnalyser *analyser, UIRModule *module, UIRValue *inst);

RIRValueId getInstFromResolved(UIRAnalyser *analyser, UIRResolved *resolved,
                               Option<RIRTypeId> default_type);

bool compareRawDataToType(UIRAnalyser *analyser, UIRRawDataKind kind,
                          RIRTypeId type_id);
RIRValue *autoCast(UIRAnalyser *analyser, RIRValue *src, RIRTypeId dst);
RIRConstant uirRawDataToRIRConstant(UIRRawData raw_data);
