#include <memory>

#include "terminal.hpp"
#include "window.hpp"
#include "layer.hpp"
#include "task.hpp"

Terminal::Terminal() {
  window_ = std::make_shared<ToplevelWindow>(
    kColumns * 8 + 8 + ToplevelWindow::kMarginX,
    kRows * 16 + 8 + ToplevelWindow::kMarginY,
    screen_config.pixel_format,
    "Terminal"
  );

  DrawTerminal(*window_->InnerWriter(), {0, 0}, window_->InnerSize());

  layer_id_ = layer_manager->NewLayer()
    .SetWindow(window_)
    .SetDraggable(true)
    .ID();
}

void Terminal::BlinkCursor() {
  cursor_visible_ = !cursor_visible_;
  DrawCursor(cursor_visible_);
}

void Terminal::DrawCursor(bool visible) {
  const auto color = visible ? ToColor(0xFFFFFF) : ToColor(0x000000);
  const auto pos = Vector2D<int>{cursor_.x * 8 + 4, cursor_.y * 16 + 5};
  FillRectangle(*window_->InnerWriter(), pos, {7, 15}, color);
}

void TaskTerminal(uint64_t task_id, int64_t data) {
  __asm__("cli");
  Task& task = task_manager->CurrentTask();
  Terminal* terminal = new Terminal;
  layer_manager->Move(terminal->LayerID(), {100, 200});
  active_layer->Activate(terminal->LayerID());
  __asm__("sti");

  while(true) {
    __asm__("cli");
    auto msg = task.ReceiveMessage();
    if(!msg) {
      task.Sleep();
      __asm__("sti");
      continue;
    }

    switch(msg->type) {
      case Message::kTimerTimeout:
        terminal->BlinkCursor();
        {
          Message msg{Message::kLayer, task_id};
          msg.arg.layer.layer_id = terminal->LayerID();
          msg.arg.layer.op = LayerOperation::Draw;
          __asm__("cli");
          task_manager->SendMessage(1, msg);
          __asm__("sti");
        }
        break;
      default:
        break;
    } 
  }
}