#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "./asm.h"

int stack_idx;
long stack[128];

long Pop() {
  long value = stack[stack_idx];
  stack_idx--;
  return value;
}

void Push(long value) {
  stack_idx++;
  stack[stack_idx] = value;
}

extern "C" void main(int argc, char** argv) {
  int error_count = 0;

  // main呼び出し前の段階のBSS領域などの初期化処理が実装されていないため
  // グローバル変数はmain関数で初期化する 
  stack_idx = -1;

  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "+") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a + b);
      SyscallLogString(kInfo, "+");  
    } else if (strcmp(argv[i], "-") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a - b);
      SyscallLogString(kInfo, "-");  
    } else {
      long a = atol(argv[i]);
      if(a == 0) {
        error_count++;
      }

      Push(a);
      SyscallLogString(kInfo, "#");   
    }

    if(stack_idx < 0 || stack_idx >= (sizeof(stack) / sizeof(stack[0]))) {
      printf("calculate stack is overflow.\n");
      SyscallExit(1);
    }
  }

  if(error_count > 0) {
    printf("there is errors");
    SyscallExit(1);
  } else {
    long result = Pop();
    printf("result = %ld\n", result);
    SyscallExit(result);
  }
}