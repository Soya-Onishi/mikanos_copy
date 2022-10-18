#include <cstdint>
#include <cstddef>
#include "frame_buffer_config.hpp"
#include "font.hpp"

struct PixelColor {
  uint8_t r, g, b;
};

class PixelWriter {
  public:
    PixelWriter(const FrameBufferConfig& config) : config_{config} {}
    virtual ~PixelWriter() = default;
    virtual void Write(int x, int y, const PixelColor& c) = 0;

  protected:
    uint8_t* PixelAt(int x, int y) {
      return config_.frame_buffer + 4 * (config_.pixels_per_scanline * y + x);
    }

  private:
    const FrameBufferConfig& config_;
};

class RGBResv8BitPerColorPixelWriter : public PixelWriter {
  public:
    using PixelWriter::PixelWriter;

    virtual void Write(int x, int y, const PixelColor& c) override {
      auto p = PixelAt(x, y);
      p[0] = c.r;
      p[1] = c.g;
      p[2] = c.b;
    }
};

class BGRResv8BitPerColorPixelWriter : public PixelWriter {
  public:
    using PixelWriter::PixelWriter;

    virtual void Write(int x, int y, const PixelColor& c) override {
      auto p = PixelAt(x, y);
      p[0] = c.b;
      p[1] = c.g;
      p[2] = c.r;
    }
};

void* operator new(size_t size, void* buf) {
  return buf;
}

void operator delete(void* obj) noexcept {}

void WriteAscii(PixelWriter& writer, int x, int y, char c, const PixelColor& color);

void WriteAscii(PixelWriter& writer, int x, int y, char c, const PixelColor& color) {
  if(c != 'A') {
    return;
  }

  for(int dy = 0; dy < 16; dy++) {
    for(int dx = 0; dx < 8; dx++) {
      if((kFontA[dy] << dx) & 0x80) {
        writer.Write(x + dx, y + dy, color);
      }
    }
  }
}

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

  WriteAscii(*pixel_writer, 50, 50, 'A', {0, 0, 0});
  WriteAscii(*pixel_writer, 58, 50, 'A', {0, 0, 0});

  halt();
} 