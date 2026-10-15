#pragma once

#include "optional.hpp"
#include <iostream>

template <typename T> struct Inline {
private:
  enum { Absent, _Inline, Stub } m_kind;
  T m_value;

public:
  Inline() : m_kind(Inline::Absent) {}
  Inline(Option<T> data) {
    if (data.isSome()) {
      this->m_kind = Inline::_Inline;
      this->m_value = data.get();
    } else {
      this->m_kind = Inline::Stub;
    }
  }

  inline bool isInline() { return this->m_kind == Inline::_Inline; }
  inline bool isStub() { return this->m_kind == Inline::Stub; }
  inline bool isAbsent() { return this->m_kind == Inline::Absent; }
  inline T get() {
    if (!this->isInline()) {
      std::cerr << "Cannot access optional with no data\n";
      std::abort();
    }

    return this->m_value;
  }

  inline void setInline(T value) {
    this->m_value = value;
    this->m_kind = Inline::_Inline;
  }
  inline void setStub() { this->m_kind = Inline::Stub; }
  inline void setAbsent() { this->m_kind = Inline::Absent; }
};
