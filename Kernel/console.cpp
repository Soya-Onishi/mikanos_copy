#include <cstring>
#include <memory>

#include "font.hpp"
#include "console.hpp"
#include "layer.hpp"
#include "window.hpp"

Console::Console(const PixelColor& fg_color, const PixelColor& bg_color)
  : writer_{nullptr}, fg_color_{fg_color}, bg_color_{bg_color}, buffer_{}, cursor_row_{0}, cursor_column_{0} {
    
  }

// 画面を初期化する。
void Console::Clear() {
  for(int y = 0; y < writer_->Height(); y++) {
    for(int x = 0; x < writer_->Width(); x++) {
      writer_->Write(Vector2D<int>{x, y}, bg_color_);
    }    
  }

  for(int row = 0; row < kRows; row++) {
    memset(buffer_[row], 0, kColumns + 1);
  }
}

void Console::PutString(const char* s) {
  while(*s) {
    if(*s == '\n') {
      Newline();
      // p.113内のコード例のバグ修正。
      // buffer_は行あたりkColumns + 1の領域を確保しているので、
      // kColumnsまで描画できる。
    } else if(cursor_column_ < kColumns){
      auto pos = Vector2D<int>{cursor_column_ * 8, cursor_row_ * 16};
      WriteAscii(*writer_, pos, *s, fg_color_);
      buffer_[cursor_row_][cursor_column_] = *s;
      cursor_column_++;
    }

    s++;
  }

  if(layer_manager) {
    layer_manager->Draw();
  }
}

void Console::SetWriter(PixelWriter* writer) {
  if(writer == writer_) {
    return;
  }

  writer_ = writer;
  window_.reset();
  Refresh();
}

void Console::SetWindow(const std::shared_ptr<Window>& window) {
  if(window == window_) {
    return;
  }

  window_ = window;
  writer_ = window->Writer();
  Refresh();
}

void Console::Newline() {
  cursor_column_ = 0;  
  if(cursor_row_ < kRows - 1) {
    cursor_row_++;
    return;
  } 
  
  if(window_) {
    Rectangle<int> move_src{{0, 16}, {8 * kColumns, 16 * (kRows - 1)}};
    window_->Move({0, 0}, move_src);
    FillRectangle(*writer_, {0, 16 * (kRows - 1)}, {8 * kColumns, 16}, bg_color_);
  } else {
    FillRectangle(*writer_, {0, 0}, {8 * kColumns, 16 * kRows}, bg_color_);
    for(int row = 0; row < kRows - 1; row++) {
      memcpy(buffer_[row], buffer_[row + 1], kColumns + 1);
      WriteString(*writer_, Vector2D<int>{0, 16 * row}, buffer_[row], fg_color_);
    }
    memset(buffer_[kRows - 1], 0, kColumns + 1);
  }
}

void Console::Refresh() {
  for(int row = 0; row < kRows; row++) {
    auto pos = Vector2D<int>{0, row * 16};
    WriteString(*writer_, pos, buffer_[row], fg_color_);
  }
}