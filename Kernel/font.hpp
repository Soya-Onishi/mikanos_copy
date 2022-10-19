#pragma once

#include <cstdint>
#include "graphics.hpp"

const uint8_t kFontA[16] = {
  0b00000000,
  0b00011000,
  0b00011000,
  0b00011000,
  0b00011000,
  0b00100100,
  0b00100100,
  0b00100100,
  0b00100100,
  0b01111110,
  0b01000010,
  0b01000010,
  0b01000010,
  0b11100111
};

void WriteAscii(PixelWriter& writer, int x, int y, char c, const PixelColor& color);
void WriteString(PixelWriter& writer, int x, int y, const char* s, const PixelColor& color);