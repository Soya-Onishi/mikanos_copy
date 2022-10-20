#include <cstdint>
#include <cstddef>
#include <cstdio>
#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"

void* operator new(size_t size, void* buf) {
  return buf;
}

void operator delete(void* obj) noexcept {}

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
char console_buf[sizeof(Console)];

Console* console;
PixelWriter* pixel_writer;

void halt() {
  while(1) {
    __asm__("hlt");
  }
}

int printk(const char* fmt, ...) {
  va_list ap;
  int result;
  char s[1024];

  va_start(ap, fmt);
  result = vsprintf(s, fmt, ap);
  va_end(ap);

  console->PutString(s);
  return result;
}

extern "C" void kernel_main(
  const FrameBufferConfig& frame_buffer_config
) {  
  switch(frame_buffer_config.pixel_format) {
    case kPixelRGBResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf) RGBResv8BitPerColorPixelWriter(frame_buffer_config);
      break;
    case kPixelBGRResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf) BGRResv8BitPerColorPixelWriter(frame_buffer_config);
      break;
    default:
      halt();   
  }
  // キャッシュの関係でxよりyをループの外側にしたほうがいいと考えた。
  for(int y = 0; y < frame_buffer_config.vertical_resolution; y++) {
    for(int x = 0; x < frame_buffer_config.horizontal_resolution; x++) {
      pixel_writer->Write(x, y, { 255, 255, 255 });      
    }
  }

  for(int y = 0; y < 100; y++) {
    for(int x = 0; x < 200; x++) {
      pixel_writer->Write(x, y, { 0, 255, 0 });      
    }
  }

  console = new(console_buf) Console(*pixel_writer, {255, 255, 255}, {0, 0, 0});
  console->Clear();
  // 90文字の文字列
  const char* numbers = "123456789112345678921234567893123456789412345678951234567896123456789712345678981234567899\n";
  for(int row = 0; row < console->kRows + 1; row++) {
    console->PutString(numbers);    
  }
  for(int row = 0; row < 16; row++) {
    printk("printk: %d\n", row);
  }
    
  halt();
} 