#include <optional>
#include "window.hpp"
#include "graphics.hpp"
#include "font.hpp"
#include "logger.hpp"
#include "frame_buffer.hpp"

const int kCloseButtonWidth = 16;
const int kCloseButtonHeight = 14;
const char close_button[kCloseButtonHeight][kCloseButtonWidth + 1] = {
    "...............@",
    ".:::::::::::::$@",
    ".:::::::::::::$@",
    ".:::@@::::@@::$@",
    ".::::@@::@@:::$@",
    ".:::::@@@@::::$@",
    ".::::::@@:::::$@",
    ".:::::@@@@::::$@",
    ".::::@@::@@:::$@",
    ".:::@@::::@@::$@",
    ".:::::::::::::$@",
    ".:::::::::::::$@",
    ".$$$$$$$$$$$$$$@",
    "@@@@@@@@@@@@@@@@",
};

Window::Window(int width, int height, PixelFormat shadow_format) : width_{width}, height_{height}
{
  data_.resize(height);
  for (int y = 0; y < height; y++)
  {
    data_[y].resize(width);
  }

  FrameBufferConfig config{};
  config.frame_buffer = nullptr;
  config.horizontal_resolution = width;
  config.vertical_resolution = height;
  config.pixel_format = shadow_format;

  if (auto err = shadow_buffer_.Initialize(config))
  {
    Log(kError, "failed to initialize shadow buffer: %s at %s:%d\n", err.Name(), err.File(), err.Line());
  }
}

void Window::DrawTo(FrameBuffer &dst, Vector2D<int> pos, const Rectangle<int> &area)
{
  if (!transparent_color_)
  {
    Rectangle<int> window_area{pos, Size()};
    Rectangle<int> intersection = area & window_area;
    auto copy_area = Rectangle<int>{
        intersection.pos - pos,
        intersection.size};
    dst.Copy(intersection.pos, shadow_buffer_, copy_area);
    return;
  }

  const auto tc = transparent_color_.value();
  auto &writer = dst.Writer();
  auto height_init = pos.y < 0 ? -pos.y : 0;
  auto height_end = pos.y + Height() > writer.Height() ? writer.Height() - pos.y : Height();
  auto width_init = pos.x < 0 ? -pos.x : 0;
  auto width_end = pos.x + Width() > writer.Width() ? writer.Width() - pos.x : Width();
  for (
      int y = height_init; y < height_end; y++)
  {
    for (int x = width_init; x < width_end; x++)
    {
      const auto c = At(x, y);

      if (tc != c)
      {
        writer.Write(pos + Vector2D<int>{x, y}, c);
      }
    }
  }
}

void Window::Write(Vector2D<int> pos, PixelColor c)
{
  data_[pos.y][pos.x] = c;
  shadow_buffer_.Writer().Write(pos, c);
}

void Window::SetTransparentColor(std::optional<PixelColor> c)
{
  transparent_color_ = c;
}

void Window::Move(Vector2D<int> dst_pos, const Rectangle<int> &src)
{
  shadow_buffer_.Move(dst_pos, src);
}

Window::WindowWriter *Window::Writer()
{
  return &writer_;
}

PixelColor &Window::At(int x, int y)
{
  return data_[y][x];
}

const PixelColor &Window::At(int x, int y) const
{
  return data_[y][x];
}

int Window::Width() const
{
  return width_;
}

int Window::Height() const
{
  return height_;
}

Vector2D<int> Window::Size() const
{
  return {
      width_,
      height_};
}

ToplevelWindow::ToplevelWindow(int width, int height, PixelFormat shadow_format, const std::string &title) : Window{width, height, shadow_format}, title_{title}
{
  DrawWindow(*Writer(), title_.c_str());
}

void ToplevelWindow::Activate()
{
  Window::Activate();
  DrawWindowTitle(*Writer(), title_.c_str(), true);
}

void ToplevelWindow::Deactivate()
{
  Window::Deactivate();
  DrawWindowTitle(*Writer(), title_.c_str(), false);
}

Vector2D<int> ToplevelWindow::InnerSize() const
{
  return Size() - kTopLeftMargin - kBottomRightMargin;
}

void DrawCloseButton(PixelWriter &writer, int win_width)
{
  for (int y = 0; y < kCloseButtonHeight; y++)
  {
    for (int x = 0; x < kCloseButtonWidth; x++)
    {
      PixelColor c = ToColor(0xFFFFFF);
      switch (close_button[y][x])
      {
      case '@':
        c = ToColor(0x000000);
        break;
      case '$':
        c = ToColor(0x848484);
        break;
      case ':':
        c = ToColor(0xC6C6C6);
        break;
      }

      writer.Write({win_width - 5 - kCloseButtonWidth + x, 5 + y}, c);
    }
  }
}

void DrawTextBox(PixelWriter& writer, Vector2D<int> pos, Vector2D<int> size, PixelColor background, PixelColor border_light, PixelColor border_dark) {
  auto fill_rect = 
    [&writer](Vector2D<int> pos, Vector2D<int> size, PixelColor c) {
      FillRectangle(writer, pos, size, c);
    };

  fill_rect(pos + Vector2D<int>{1, 2}, size - Vector2D<int>{2, 2}, background);
  fill_rect(pos,                       {size.x, 1},                border_light);
  fill_rect(pos, {1, size.y}, border_light);
  fill_rect(pos + Vector2D<int>{0, size.y}, {size.x, 1}, border_dark);
  fill_rect(pos + Vector2D<int>{size.x, 0}, {1, size.y}, border_dark);
}

void DrawWindowTitle(PixelWriter &writer, const char *title, bool active)
{
  const auto win_w = writer.Width();
  uint32_t bg_color = active ? 0x000084 : 0x848484;
  FillRectangle(writer, {3, 3}, {win_w - 6, 18}, ToColor(bg_color));
  WriteString(writer, {24, 4}, title, ToColor(0xFFFFFF));

  DrawCloseButton(writer, win_w);
}

void DrawWindow(PixelWriter &writer, const char *title)
{
  auto fill_rect = [&writer](Vector2D<int> pos, Vector2D<int> size, uint32_t c)
  {
    FillRectangle(writer, pos, size, ToColor(c));
  };

  const auto win_w = writer.Width();
  const auto win_h = writer.Height();
  fill_rect({0, 0}, {win_w, win_h}, 0xC6C6C6);
  DrawWindowTitle(writer, title, false); 
}

void DrawTextBox(PixelWriter& writer, Vector2D<int> pos, Vector2D<int> size) {
  DrawTextBox(writer, pos, size, ToColor(0xFFFFFF), ToColor(0xC6C6C6), ToColor(0x848484));
}

void DrawTerminal(PixelWriter& writer, Vector2D<int> pos, Vector2D<int> size) {
  DrawTextBox(writer, pos, size, ToColor(0x000000), ToColor(0xC6C6C6), ToColor(0x848484));
}