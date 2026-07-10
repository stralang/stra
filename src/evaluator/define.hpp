#include "evaluator.hpp"
#include <sstream>

#define expect(ok, srcloc, msg)                                                \
  if (!(ok)) {                                                                 \
    std::ostringstream os;                                                     \
    os << msg;                                                                 \
    std::string cpp_str = os.str();                                            \
    String m = {.len = cpp_str.size(), .ptr = (uint8_t *)cpp_str.data()};      \
    evaluator->error_func(srcloc, m);                                          \
    evaluator->error_count += 1;                                               \
  }

#define warn(srcloc, msg)                                                      \
  {                                                                            \
    std::ostringstream os;                                                     \
    os << msg;                                                                 \
    std::string cpp_str = os.str();                                            \
    String m = {.len = cpp_str.size(), .ptr = (uint8_t *)cpp_str.data()};      \
    evaluator->warning_func(srcloc, m);                                        \
    evaluator->warning_count += 1;                                             \
  }

// Base
void evaluate(Evaluator *evaluator, Node *node, Symbol *scope);

// Operator
void evaluateAssignment(Evaluator *evaluator, Node *node, Symbol *scope);
void evaluateUnary(Evaluator *evaluator, Node *node, Symbol *scope);
void evaluateBinary(Evaluator *evaluator, Node *node, Symbol *scope);

// Type
bool compareTypes(Type *lhs, Type *rhs);

/// Checks if `src` can auto convert to `dst`
/// Returns `dst` if it can convert, otherwise `src`
Type *autoConvert(Evaluator *evaluator, Type *src, Type *dst);

// Converts `src` to `dst`, injecting a cast if necessary
void autoCast(Evaluator *evaluator, Node *src, Type *dst);

void fixUntyped(Evaluator *evaluator, Node *node, Type *real);

// Function
void evaluateFunction(Evaluator *evaluator, Node *node, Symbol *scope);
void evaluateCall(Evaluator *evaluator, Node *node, Symbol *scope);

// Builtin
Value getBuiltinValue(TypeCache *type_cache, String name);
void populateBuiltinVariable(Evaluator *evaluator, Node *node, Symbol *scope,
                             String name);
void evaluateBuiltinFunction(Evaluator *evaluator, Node *node, Symbol *scope,
                             String name);

// Desugar
void desugarModifyAssign(Evaluator *evaluator, Node *node, Symbol *scope);
Symbol *desugarForIn(Evaluator *evaluator, Node *node, Symbol *for_scope,
                     Symbol *parent_scope);
