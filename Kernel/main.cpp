#include <cstdint>
#include "frame_buffer_config.hpp"

struct PixelColor {
  uint8_t r, g, b;
};

int WritePixel(const FrameBufferConfig& config, int x, int y, const PixelColor& c);

int WritePixel(
  const FrameBufferConfig& config,
  int x,
  int y,
  const PixelColor& c
) {
  const int pixel_position = config.pixels_per_scanline * y + x;
  uint8_t* p = &config.frame_buffer[pixel_position * 4];

  switch(config.pixel_format) {
    case kPixelRGBResv8BitPerColor:      
      p[0] = c.r;
      p[1] = c.g;
      p[2] = c.b;
      break;
    case kPixelBGRResv8BitPerColor:
      p[0] = c.b;
      p[1] = c.g;
      p[2] = c.r;
      break;
    default:
      return -1;
  }

  return 0;
}

extern "C" void kernel_main(
  const FrameBufferConfig& frame_buffer_config
) {
  // キャッシュの関係でxよりyをループの外側にしたほうがいいと考えた。
  for(int y = 0; y < frame_buffer_config.vertical_resolution; y++) {
    for(int x = 0; x < frame_buffer_config.horizontal_resolution; x++) {
      WritePixel(frame_buffer_config, x, y, {255, 255, 255});
    }
  }

  for(int y = 0; y < 100; y++) {
    for(int x = 0; x < 200; x++) {
      WritePixel(frame_buffer_config, x + 100, y + 100, { 0, 255, 0 });
    }
  }

  while(1) {
    __asm__("hlt");
  }
} 