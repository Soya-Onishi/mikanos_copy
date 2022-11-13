#pragma once

#include <cstdint>
#include <memory>
#include <array>
#include <deque>

#include "window.hpp"
#include "graphics.hpp"

class Terminal {
  public:
    static const int kRows = 15;
    static const int kColumns = 60;
    static const int kLineMax = 128;

    Terminal();
    unsigned int LayerID() const { return layer_id_; }
    Rectangle<int> BlinkCursor();
    Rectangle<int> InputKey(uint8_t modifier, uint8_t keycode, char ascii);
    void Print(const char* s);
    void Print(const char c);
    void ExecuteLine();
  
  private:
    std::shared_ptr<ToplevelWindow> window_;
    unsigned int layer_id_;
    
    Vector2D<int> cursor_{0, 0};
    bool cursor_visible_{false};
    Rectangle<int> DrawCursor(bool visible);
    Vector2D<int> CalcCursorPos() const;

    int linebuf_index_{0};
    std::array<char, kLineMax> linebuf_{};
    void Scroll_OneLine();

    std::deque<std::array<char, kLineMax>> cmd_history_{};
    int cmd_history_index_{-1};
    Rectangle<int> HistoryUpDown(int direction);
};

void TaskTerminal(uint64_t task_id, int64_t data);