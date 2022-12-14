#pragma once

#include <memory>
#include "graphics.hpp"
#include "window.hpp"

class Console {
  public:
    static const int kRows = 25;
    static const int kColumns = 80;

    Console(const PixelColor& fg_color, const PixelColor& bg_color);
    void PutString(const char* s);
    void SetWriter(PixelWriter* writer);
    void SetWindow(const std::shared_ptr<Window>& window);
    void SetLayerID(const unsigned int layer_id);
    void Clear();
    unsigned int LayerID();

  private:
    void Newline();
    void Refresh();

    PixelWriter* writer_;
    std::shared_ptr<Window> window_;
    unsigned int layer_id_;
    const PixelColor& fg_color_;
    const PixelColor& bg_color_;
    char buffer_[kRows][kColumns + 1];
    int cursor_row_;
    int cursor_column_;    
};

inline Console* console;

void InitializeConsole();