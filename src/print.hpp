#pragma once

#include "ast.hpp"
#include "operator.hpp"
#include "token.hpp"
#include <ostream>

std::ostream &operator<<(std::ostream &os, const String &str);

std::ostream &operator<<(std::ostream &os, const SrcLoc &location);
std::ostream &operator<<(std::ostream &os, const Operator &op);
std::ostream &operator<<(std::ostream &os, const UnaryOperator &op);
std::ostream &operator<<(std::ostream &os, const TokenKind &kind);
std::ostream &operator<<(std::ostream &os, const Token &token);

std::ostream &operator<<(std::ostream &os, const NodeKind &kind);
std::ostream &operator<<(std::ostream &os, const Node &node);
