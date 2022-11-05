#include <cstdlib>
#include <cstring>

#include "acpi.hpp"
#include "logger.hpp"

namespace {
  template <typename T>
  uint8_t SumBytes(const T* data, size_t bytes) {
    return SumBytes(reinterpret_cast<const uint8_t*>(data), bytes);
  }

  template <>
  uint8_t SumBytes(const uint8_t* data, size_t bytes) {
    uint8_t sum = 0;
    for (size_t i = 0; i < bytes; i++) {
      sum += data[i];
    }

    return sum;
  }
}

void acpi::Initialize(const acpi::RSDP& rsdp) {
  if(!rsdp.IsValid()) {
    Log(kError, "RSDP is not valid");
    exit(1);
  }
}

bool acpi::RSDP::IsValid() const {
  if(strncmp(this->signature, "RSD PTR ", 8) != 0) {
    Log(kError, "invalid signature: %.8s\n", this->signature);
    return false;
  }

  if(this->revision != 2) {
    Log(kError, "ACPI revision must be 2: %d\n", this->revision);
    return false;
  }

  if(auto sum = SumBytes(this, 20); sum != 0) {
    Log(kError, "sum of 20bytes must be 0: %d\n", sum);
    return false;
  }

  if(auto sum = SumBytes(this, 36); sum != 0) {
    Log(kError, "sum of 36 bytes must be 0: %d\n", sum);
    return false;
  }

  return true;
}