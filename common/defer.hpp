#pragma once

#include <algorithm>

template <typename F> struct Defer {
  F f;
  Defer(F f) : f(std::move(f)) {}
  ~Defer() { f(); }
};

template <typename F> Defer<F> makeDefer(F f) { return {f}; }

#define __DEFER_NAME_1__(a, b) a##b
#define __DEFER_NAME_2__(a, b) __DEFER_NAME_1__(a, b)
#define defer(T)                                                               \
  auto __DEFER_NAME_2__(__defer_var, __COUNTER__) = makeDefer([&]() { T; })
