#include "evaluator.hpp"

#define expect(ok, srcloc, msg)                                                \
  if (!(ok)) {                                                                 \
    std::cerr << srcloc << " " << msg << '\n';                                 \
    node->value.type = nullptr;                                                \
    return;                                                                    \
  }

// Base
void evaluate(Evaluator *evaluator, Node *node, Symbol *scope);

// Operator
void evaluateUnary(Evaluator *evaluator, Node *node, Symbol *scope);
void evaluateBinary(Evaluator *evaluator, Node *node, Symbol *scope);

// Type
bool compareTypes(Type *lhs, Type *rhs);

/// Checks if `src` can auto convert to `dst`
/// Returns `dst` if it can convert, otherwise `src`
Type *autoConvert(Evaluator *evaluator, Type *src, Type *dst);

// Function
void evaluateFunction(Evaluator *evaluator, Node *node, Symbol *scope);
void evaluateCall(Evaluator *evaluator, Node *node, Symbol *scope);

// Builtin
Value getBuiltinValue(TypeCache *type_cache, String name);
