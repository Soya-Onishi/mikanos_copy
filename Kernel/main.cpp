#include <cstdint>
#include <cstddef>
#include <cstdio>
#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "font.hpp"

void* operator new(size_t size, void* buf) {
  return buf;
}

void operator delete(void* obj) noexcept {}

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

void halt() {
  while(1) {
    __asm__("hlt");
  }
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

  char buf[128];
  sprintf(buf, "1 + 2 = %d", 1 + 2);
  WriteString(*pixel_writer, 50, 50, "Hello World!", {0, 0, 255});
  WriteString(*pixel_writer, 50, 66, buf, {0, 0, 0});
  
  halt();
} 