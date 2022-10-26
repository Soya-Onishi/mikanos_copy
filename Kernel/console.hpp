#pragma once

#include "graphics.hpp"

class Console {
  public:
    static const int kRows = 25;
    static const int kColumns = 80;

    Console(const PixelColor& fg_color, const PixelColor& bg_color);
    void PutString(const char* s);
    void SetWriter(PixelWriter* writer);
    void Clear();

  private:
    void Newline();
    void Refresh();

    PixelWriter* writer_;
    const PixelColor& fg_color_;
    const PixelColor& bg_color_;
    char buffer_[kRows][kColumns + 1];
    int cursor_row_;
    int cursor_column_;    
};