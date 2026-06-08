#include "general.hpp"
#include <iostream>

extern TargetABI ABIcreateSystemVAmd64();

TargetABI ABIcreateTarget(ABI abi) {
  switch (abi) {
  case ABI::SystemV_Amd64: {
    return ABIcreateSystemVAmd64();
  }
  }

  std::cerr << "Cannot create abi target for `" << (int)abi << "`\n";
  std::abort();
}
