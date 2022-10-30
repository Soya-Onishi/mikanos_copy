#include <cstdint>
#include "timer.hpp"
#include "interrupt.hpp"

namespace {
  const uint32_t kCountMax = 0xFFFFFFFFu;
  volatile uint32_t& lvt_timer = *reinterpret_cast<uint32_t*>(0xFEE00320u);
  volatile uint32_t& initial_count = *reinterpret_cast<uint32_t*>(0xFEE00380u);
  volatile uint32_t& current_count = *reinterpret_cast<uint32_t*>(0xFEE00390u);
  volatile uint32_t& divide_config = *reinterpret_cast<uint32_t*>(0xFEE003E0u);
}

void InitializeAPICTimer() {
  divide_config = 0b1011;
  lvt_timer = (0b010 << 16) | InterruptVector::kLAPICTimer;
  initial_count = kCountMax;
}

void StartAPICTimer() {
  initial_count = kCountMax;
}

uint32_t LAPICTimerElapsed() {
  return kCountMax - current_count;
}

void StopLAPICTimer() {
  initial_count = 0;
}