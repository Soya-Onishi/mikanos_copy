#include "graphics.hpp"
#include "font.hpp"

extern const uint8_t _binary_hankaku_bin_start;
extern const uint8_t _binary_hankaku_bin_end;
extern const uint8_t _binary_hankaku_bin_size;

const uint8_t* GetFont(char c) {
  auto index = 16 * static_cast<unsigned int>(c);
  if(index >= reinterpret_cast<uintptr_t>(&_binary_hankaku_bin_size)) {
    return nullptr;
  }

  return &_binary_hankaku_bin_start + index;
}

void WriteAscii(PixelWriter& writer, Vector2D<int> base, char c, const PixelColor& color) {
  const uint8_t* font = GetFont(c);

  for(int dy = 0; dy < 16; dy++) {
    for(int dx = 0; dx < 8; dx++) {
      if((font[dy] << dx) & 0x80) {
        auto pos = Vector2D<int>{dx, dy};
        writer.Write(base + pos, color);
      }
    }
  }
}

void WriteString(PixelWriter& writer, Vector2D<int> pos, const char* s, const PixelColor& color) {
  for(int i = 0; s[i] != '\0'; i++) {
    auto base = Vector2D<int> { pos.x + i * 8, pos.y };
    WriteAscii(writer, base, s[i], color);
  }
}