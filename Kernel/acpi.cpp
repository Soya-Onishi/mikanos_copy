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

  const acpi::XSDT& xsdt = *reinterpret_cast<XSDT*>(rsdp.xsdt_address);
  if(!xsdt.header.IsValid("XSDT")) {
    Log(kError, "XSDT is not valid\n");
    exit(1);
  }

  acpi::fadt = nullptr;
  for(int i = 0; i < xsdt.Count(); i++) {
    const auto& entry = xsdt[i];
    if(entry.IsValid("FACP")) {
      acpi::fadt = reinterpret_cast<const FADT*>(&entry);
      break;
    }
  }

  if(fadt == nullptr) {
    Log(kError, "FADT is not found\n");
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

bool acpi::DescriptionHeader::IsValid(const char* expected_signature) const {
  if(strncmp(this->signature, expected_signature, 4) != 0) { 
    Log(kError, "invalid signature: %.4s expected: %.4s\n", this->signature, expected_signature);
    return false;
  }

  if(auto sum = SumBytes(this, this->length); sum != 0) {
    Log(kError, "sum of %u bytes must be 0: %d\n", sum);
    return false;
  }

  return true;
}

const acpi::DescriptionHeader& acpi::XSDT::operator[](size_t i) const {
  auto entries = reinterpret_cast<const uint64_t*>(&this->header + 1);
  return *reinterpret_cast<acpi::DescriptionHeader*>(entries[i]);
}

size_t acpi::XSDT::Count() const {
  return (this->header.length - sizeof(DescriptionHeader)) / sizeof(uint64_t);
}