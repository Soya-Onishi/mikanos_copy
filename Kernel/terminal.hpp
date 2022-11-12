#pragma once

#include <cstdint>
#include <memory>

#include "window.hpp"
#include "graphics.hpp"

class Terminal {
  public:
    static const int kRows = 15;
    static const int kColumns = 60;

    Terminal();
    unsigned int LayerID() const { return layer_id_; }
    void BlinkCursor();
  
  private:
    std::shared_ptr<ToplevelWindow> window_;
    unsigned int layer_id_;
    
    Vector2D<int> cursor_{0, 0};
    bool cursor_visible_{false};
    void DrawCursor(bool visible);
};

void TaskTerminal(uint64_t task_id, int64_t data);