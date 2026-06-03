#pragma once

#include "allocator.hpp"
#include "ast.hpp"

Node *astCopy(Allocator *allocator, Node *src, Symbol *scope);
