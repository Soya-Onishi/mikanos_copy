#pragma once

#include <cstdint>
#include "logger.hpp"

extern "C" {
  struct SyscallResult {
    uint64_t value;
    int error;
  };

  struct SyscallResult SyscallPutString(uint64_t, uint64_t, uint64_t);
  struct SyscallResult SyscallLogString(LogLevel, const char*);
  void SyscallExit(int code);
  struct SyscallResult SyscallOpenWindow(int w, int h, int x, int y, const char* title);
  struct SyscallResult SyscallWinWriteString(unsigned int layer_id, int x, int y, uint32_t color, const char* msg);
}