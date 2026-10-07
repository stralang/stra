#pragma once

#include "../uir.hpp"
#include "allocator.hpp"
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

void analyseBlock(UIRAnalyser *analyser, UIRBlock *block);
void analyseScope(UIRAnalyser *analyser, UIRScope *scope);

void analyse(UIRAnalyser *analyser, UIRValue *inst);
void analyseBinary(UIRAnalyser *analyser, UIRValue *inst);
void analyseUnary(UIRAnalyser *analyser, UIRValue *inst);
void analyseLookupPtr(UIRAnalyser *analyser, UIRValue *inst);
void analyseLookupValue(UIRAnalyser *analyser, UIRValue *inst);
void analyseGlobal(UIRAnalyser *analyser, UIRValue *inst);
void analyseAggregate(UIRAnalyser *analyser, UIRValue *inst);

RIRValueId getInstFromResolved(UIRAnalyser *analyser, UIRResolved *resolved,
                               Option<RIRTypeId> default_type);

bool compareRawDataToType(UIRAnalyser *analyser, UIRRawDataKind kind,
                          RIRTypeId type_id);
RIRValue *autoCast(UIRAnalyser *analyser, RIRValue *src, RIRTypeId dst);
RIRConstant uirRawDataToRIRConstant(Allocator *allocator, UIRRawData raw_data);
