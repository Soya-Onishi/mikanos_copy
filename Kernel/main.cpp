extern "C" void kernel_main() {
  while(1) {
    __asm__("hlt");
  }
}