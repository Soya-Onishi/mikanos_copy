bits 64
section .text

global halt ; void halt();
halt:
  nop
  jmp halt

global SyscallLogString
SyscallLogString:       ; void SyscallLogString(LogLevel, const char* s);
  mov eax, 0x80000000
  mov r10, rcx
  syscall
  ret