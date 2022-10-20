#include <cstring>
#include "font.hpp"
#include "console.hpp"

Console::Console(PixelWriter& writer, const PixelColor& fg_color, const PixelColor& bg_color)
  : writer_{writer}, fg_color_{fg_color}, bg_color_{bg_color}, buffer_{}, cursor_row_{0}, cursor_column_{0} {
    
  }

// 画面のコンソール範囲を初期化する。
void Console::Clear() {
  for(int y = 0; y < 16 * kRows; y++) {
    for(int x = 0; x < 8 * kColumns; x++) {
      writer_.Write(x, y, bg_color_);
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
      WriteAscii(writer_, cursor_column_ * 8, cursor_row_ * 16, *s, fg_color_);
      buffer_[cursor_row_][cursor_column_] = *s;
      cursor_column_++;
    }

    s++;
  }
}

void Console::Newline() {
  cursor_column_ = 0;
  if(cursor_row_ < kRows - 1) {
    cursor_row_++;
  } else {
    for(int y = 0; y < 16 * kRows; y++) {
      for(int x = 0; x < 8 * kColumns; x++) {
        writer_.Write(x, y, bg_color_);
      }
    }

    for(int row = 0; row < kRows - 1; row++) {
      memcpy(buffer_[row], buffer_[row + 1], kColumns + 1);      
      WriteString(writer_, 0, row * 16, buffer_[row], fg_color_);
    }
    memset(buffer_[kRows - 1], 0, kColumns + 1);
  }
}