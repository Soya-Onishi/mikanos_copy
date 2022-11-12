#include "graphics.hpp"

void RGBResv8BitPerColorPixelWriter::Write(Vector2D<int> pos, const PixelColor& color)  {
  auto p = PixelAt(pos.x, pos.y);
  p[0] = color.r;
  p[1] = color.g;
  p[2] = color.b;
}

void BGRResv8BitPerColorPixelWriter::Write(Vector2D<int> pos, const PixelColor& color) {
  auto p = PixelAt(pos.x, pos.y);
  p[0] = color.b;
  p[1] = color.g;
  p[2] = color.r;
}

void DrawRectangle(PixelWriter& writer, const Vector2D<int>& pos, const Vector2D<int>& size, const PixelColor& color) {
  for(int x = 0; x < size.x; x++) {
    
    writer.Write(pos + Vector2D<int>{x, 0}         , color);
    writer.Write(pos + Vector2D<int>{x, size.y - 1}, color);    
  }

  for(int y = 0; y < size.y; y++) {
    writer.Write(pos + Vector2D<int>{        0, y}, color);
    writer.Write(pos + Vector2D<int>{size.x -1, y}, color);    
  }
}

void FillRectangle(PixelWriter& writer, const Vector2D<int>& pos, const Vector2D<int>& size, const PixelColor& color) {
  for(int y = 0; y < size.y; y++) {
    for(int x = 0; x < size.x; x++) {
      writer.Write(pos + Vector2D<int>{x, y}, color);      
    }
  }
}

void DrawDesktop(PixelWriter& writer) {
  const auto width = writer.Width();
  const auto height = writer.Height();
  FillRectangle(writer,
                {0, 0},
                {width, height - 50},
                {255, 255, 255});
  FillRectangle(writer,
                {0, height - 50},
                {width, 50},
                {1, 8, 17});
  FillRectangle(writer,
                {0, height - 50},
                {width / 5, 50},
                {80, 80, 80});
  DrawRectangle(writer,
                {10, height - 40},
                {30, 30},
                {160, 160, 160});
}

namespace {
  char screen_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];  
}

void InitializeGraphics(const FrameBufferConfig& config) {
  ::screen_config = config;

  switch(config.pixel_format) {
    case kPixelRGBResv8BitPerColor:
      screen_writer = new(screen_writer_buf) RGBResv8BitPerColorPixelWriter(config);
      break;
    case kPixelBGRResv8BitPerColor:
      screen_writer = new(screen_writer_buf) BGRResv8BitPerColorPixelWriter(config);
      break;
    default:
      exit(1);
  }
}

Vector2D<int> ScreenSize() {
  return Vector2D<int> {
    static_cast<int>(screen_config.horizontal_resolution),
    static_cast<int>(screen_config.vertical_resolution)
  };
}

