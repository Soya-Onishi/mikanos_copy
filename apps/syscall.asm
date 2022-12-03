bits 64
section .text

global SyscallLogString
SyscallLogString:       ; void SyscallLogString(LogLevel, const char* s);
  mov eax, 0x80000000
  mov r10, rcx
  syscall
  ret

global SyscallPutString
SyscallPutString:       ; void SyscallLogString(LogLevel, const char* s);
  mov eax, 0x80000001
  mov r10, rcx
  syscall
  ret

global SyscallExit
SyscallExit:
  mov eax, 0x80000002
  mov r10, rcx
  syscall
  ret

global SyscallOpenWindow
SyscallOpenWindow:
  mov eax, 0x80000003
  mov r10, rcx
  syscall
  ret

global SyscallWinWriteString
SyscallWinWriteString:
  mov eax, 0x80000004
  mov r10, rcx
  syscall
  ret