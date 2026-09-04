#pragma once

#include <iostream>

template <typename T> struct Option {
private:
  bool some = false;
  T data;

public:
  static Option<T> from(T value) {
    Option<T> opt;
    opt.some = true;
    opt.data = value;
    return opt;
  }

  void setSome(T value) {
    this->data = value;
    this->some = true;
  }
  void setNone() { this->some = false; }

  inline bool isSome() { return this->some; }
  inline bool isNone() { return !this->isSome(); }

  T *getPtr() {
    if (!this->some) {
      std::cerr << "[" << __FILE_NAME__ << ":" << __LINE__
                << "] Cannot read from empty optional\n";
      std::abort();
    }

    return &this->data;
  }
  inline T get() { return *this->getPtr(); }
};
