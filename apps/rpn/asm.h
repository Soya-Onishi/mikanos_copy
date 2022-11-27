#pragma once

#include "logger.hpp"

extern "C" {
  void halt();
  void SyscallLogString(LogLevel, const char*);
}