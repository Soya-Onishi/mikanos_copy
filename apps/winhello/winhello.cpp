#include "../syscall.h"

extern "C" void main(int argc, char** argv) {
  auto [layer_id, err] = SyscallOpenWindow(200, 100, 10, 10, "winhello");
  if(err) {
    SyscallExit(err);
  }

  SyscallWinWriteString(layer_id,  7, 24, 0xC00000, "Hello World");
  SyscallWinWriteString(layer_id, 24, 40, 0x00C000, "Hello World");
  SyscallWinWriteString(layer_id, 40, 56, 0x0000C0, "Hello World");

  SyscallExit(0);
}