int strcmp(const char* a, const char* b) {
  int i = 0;
  for(; a[i] != 0 && b[i] != 0; ++i) {
    if(a[i] != b[i]) {
      return a[i] - b[i];
    }
  }

  return a[i] - b[i];
}

long atol(const char* s) {
  long v = 0;
  for(int i = 0; s[i] != 0; i++) {
    v = v * 10 + (s[i] - '0');
  }

  return v;
}

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
      Push(a);
    }

    if(stack_idx < 0 || stack_idx >= (sizeof(stack) / sizeof(stack[0]))) {
      return 0;
    }
  }

  return static_cast<int>(Pop());
}