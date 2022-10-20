#include "graphics.hpp"

void RGBResv8BitPerColorPixelWriter::Write(int x, int y, const PixelColor& color)  {
  auto p = PixelAt(x, y);
  p[0] = color.r;
  p[1] = color.g;
  p[2] = color.b;
}

void BGRResv8BitPerColorPixelWriter::Write(int x, int y, const PixelColor& color) {
  auto p = PixelAt(x, y);
  p[0] = color.b;
  p[1] = color.g;
  p[2] = color.r;
}

void DrawRectangle(PixelWriter& writer, const Vector2D<int>& pos, const Vector2D<int>& size, const PixelColor& color) {
  for(int x = 0; x < size.x; x++) {
    writer.Write(pos.x + x, pos.y             , color);
    writer.Write(pos.x + x, pos.y + size.y - 1, color);
  }

  for(int y = 0; y < size.y; y++) {
    writer.Write(pos.x             , pos.y + y, color);
    writer.Write(pos.x + size.x - 1, pos.y + y, color);
  }
}

void FillRectangle(PixelWriter& writer, const Vector2D<int>& pos, const Vector2D<int>& size, const PixelColor& color) {
  for(int y = 0; y < size.y; y++) {
    for(int x = 0; x < size.x; x++) {
      writer.Write(pos.x + x, pos.y + y, color);
    }
  }
}