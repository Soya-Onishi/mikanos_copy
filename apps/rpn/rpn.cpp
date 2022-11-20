#include <cstring>
#include <cstdlib>

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

extern "C" int main(int argc, char** argv) {
  int error_count = 0;

  // main呼び出し前の段階のBSS領域などの初期化処理が実装されていないため
  // グローバル変数はmain関数で初期化する 
  stack_idx = -1;

  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "+") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a + b);
    } else if (strcmp(argv[i], "-") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a - b);
    } else {
      long a = atol(argv[i]);
      if(a == 0) {
        error_count++;
      }

      Push(a);
    }

    if(stack_idx < 0 || stack_idx >= (sizeof(stack) / sizeof(stack[0]))) {
      return 0;
    }
  }

  if(error_count > 0) {
    return error_count;
  } else {
    return static_cast<int>(Pop());
  }
}