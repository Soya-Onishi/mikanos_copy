bits 64
section .text

extern main

global halt ; void halt();
halt:
  nop
  jmp halt

global _start
_start:
  call main
  