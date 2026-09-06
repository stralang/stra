#pragma once

#include <iostream>
#include <type_traits>

template <typename T> struct Option {
private:
  bool m_some;
  T m_value;

public:
  Option() : m_some(false) {}
  Option(T value) : m_value(value) {
    this->m_some =
        !std::is_pointer_v<T> || value != nullptr; // convert `nullptr` to none
  }

  inline bool isSome() { return this->m_some; }
  inline bool isNone() { return !this->isSome(); }
  inline T get() {
    if (this->isNone()) {
      std::cerr << "Cannot access optional with no data\n";
      std::abort();
    }

    return this->m_value;
  }

  inline void setSome(T value) {
    this->m_value = value;
    this->m_some = true;
  }
  inline void setNone() { this->m_some = false; }
};
