#include "mirgen.hpp"

void MIRGen::generate() {
  this->node_to_value.init(this->allocator, 32);

  this->module.allocator = this->allocator;
  this->module.instructions.init(this->allocator, 32);
}

void MIRGen::deinit() {
  this->node_to_value.deinit();
  this->module.instructions.deinit();
}
