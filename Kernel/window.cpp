#include <optional>
#include "window.hpp"
#include "graphics.hpp"
#include "logger.hpp"
#include "frame_buffer.hpp"

Window::Window(int width, int height, PixelFormat shadow_format) : width_{width}, height_{height} {
  data_.resize(height);
  for(int y = 0; y < height; y++) {
    data_[y].resize(width);
  }

  FrameBufferConfig config{};
  config.frame_buffer = nullptr;
  config.horizontal_resolution = width;
  config.vertical_resolution = height;
  config.pixel_format = shadow_format;

  if(auto err = shadow_buffer_.Initialize(config)) {
    Log(kError, "failed to initialize shadow buffer: %s at %s:%d\n", err.Name(), err.File(), err.Line());
  }
}

void Window::DrawTo(FrameBuffer& dst, Vector2D<int> pos, const Rectangle<int>& area) {
  if(!transparent_color_) {
    Rectangle<int> window_area{pos, Size()};
    Rectangle<int> intersection = area & window_area;
    auto copy_area = Rectangle<int> {
      intersection.pos - pos,
      intersection.size
    };
    dst.Copy(intersection.pos, shadow_buffer_, copy_area);
    return;
  }

  const auto tc = transparent_color_.value();  
  auto& writer = dst.Writer();
  auto height_init = pos.y < 0 ? -pos.y : 0;
  auto height_end  = pos.y + Height() > writer.Height() ? writer.Height() - pos.y : Height();
  auto width_init  = pos.x < 0 ? -pos.x : 0;
  auto width_end   = pos.x + Width() > writer.Width() ? writer.Width() - pos.x : Width();
  for(
    int y = height_init; y < height_end; y++) {
    for(int x = width_init; x < width_end; x++) {
      const auto c = At(x, y);

      if(tc != c) {
        writer.Write(pos + Vector2D<int>{x, y}, c);
      }
    }
  }
}

void Window::Write(Vector2D<int> pos, PixelColor c) {
  data_[pos.y][pos.x] = c;
  shadow_buffer_.Writer().Write(pos, c);
}

void Window::SetTransparentColor(std::optional<PixelColor> c) {
  transparent_color_ = c;
}

void Window::Move(Vector2D<int> dst_pos, const Rectangle<int>& src) {
  shadow_buffer_.Move(dst_pos, src);
}

Window::WindowWriter* Window::Writer() {
  return &writer_;
}

PixelColor& Window::At(int x, int y)  {
  return data_[y][x];
}

const PixelColor& Window::At(int x, int y) const {
  return data_[y][x];
}

int Window::Width() const {
  return width_;
}

int Window::Height() const {
  return height_;
}

Vector2D<int> Window::Size() const {
  return {
    width_,
    height_
  };
}