#include "graphics.hpp"
#include "font.hpp"

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
