#include <memory>

#include "terminal.hpp"
#include "window.hpp"
#include "layer.hpp"
#include "task.hpp"
#include "font.hpp"
#include "logger.hpp"

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

  Print("> ");
}

Rectangle<int> Terminal::BlinkCursor() {
  cursor_visible_ = !cursor_visible_;
  auto inner_area = DrawCursor(cursor_visible_); 
  auto window_area = Rectangle<int>{inner_area.pos + ToplevelWindow::kTopLeftMargin, inner_area.size};

  return window_area;
}

Rectangle<int> Terminal::DrawCursor(bool visible) {
  const auto color = visible ? ToColor(0xFFFFFF) : ToColor(0x000000);
  const auto pos = CalcCursorPos();
  const auto size = Vector2D<int>{7, 15};

  FillRectangle(*window_->InnerWriter(), pos, size, color);

  return {pos, size};
}

Vector2D<int> Terminal::CalcCursorPos() const {
  return Vector2D<int>{cursor_.x * 8 + 4, cursor_.y * 16 + 5};
}

Rectangle<int> Terminal::InputKey(uint8_t modifier, uint8_t keycode, char ascii) {
  DrawCursor(false);
  
  Rectangle<int> draw_area{ToplevelWindow::kTopLeftMargin + CalcCursorPos(), {8*2, 16}};

  switch(ascii) {
    case '\n':
      linebuf_[linebuf_index_] = 0;
      linebuf_index_ = 0;
      cursor_.x = 0;
      if(cursor_.y < kRows - 1) {
        cursor_.y++;
      } else {
        Scroll_OneLine();
      }

      ExecuteLine();
      Print("> ");

      draw_area.pos = ToplevelWindow::kTopLeftMargin;
      draw_area.size = window_->InnerSize();
      break;
    case '\b':
      if(cursor_.x > 0) {
        cursor_.x--;
        FillRectangle(*window_->InnerWriter(), CalcCursorPos(), {8, 16}, ToColor(0x000000));
        draw_area.pos = ToplevelWindow::kTopLeftMargin + CalcCursorPos();

        if(linebuf_index_ > 0) {
          linebuf_index_--;
        }
      }
      break;
    default:
      if(ascii != 0 && cursor_.x < kColumns - 1 && linebuf_index_ < kLineMax - 1) {
        linebuf_[linebuf_index_] = ascii;
        linebuf_index_++;
        WriteAscii(*window_->InnerWriter(), CalcCursorPos(), ascii, ToColor(0xFFFFFF));
        cursor_.x++;
      }
  }

  DrawCursor(true);

  return draw_area;
}

void Terminal::Scroll_OneLine() {
  Rectangle<int> move_src {
    ToplevelWindow::kTopLeftMargin + Vector2D<int>{4, 4 + 16},
    {8 * kColumns, 16 * (kRows - 1)}  
  };

  window_->Move(ToplevelWindow::kTopLeftMargin + Vector2D<int>{4, 4}, move_src);  
  FillRectangle(*window_->InnerWriter(), {4, cursor_.y * 16 + 4}, {kColumns * 8, 16}, ToColor(0x000000));
}

void Terminal::Print(const char* s) {
  DrawCursor(false);

  auto newline = [this]() {
    cursor_.x = 0;
    if(cursor_.y < kRows - 1) {
      ++cursor_.y;
    } else {
      this->Scroll_OneLine();
    }
  };

  while(*s) {
    if(*s == '\n') {
      newline();
    } else {
      WriteAscii(*window_->InnerWriter(), CalcCursorPos(), *s, ToColor(0xFFFFFF));
      if(cursor_.x == kColumns - 1) {
        newline();
      } else {
        cursor_.x++;
      }
    }
    s++;
  }

  DrawCursor(true);
}

void Terminal::ExecuteLine() {
  char* command = &linebuf_[0];
  char* first_arg = strchr(&linebuf_[0], ' ');
  if(first_arg) {
    *first_arg = 0;
    first_arg++;
  } 

  if(strcmp(command, "echo") == 0) {
    if(first_arg) {
      Print(first_arg);
    }
    Print("\n");
  } else if(strcmp(command, "clear") == 0) {
    FillRectangle(*window_->InnerWriter(), {0, 0}, window_->InnerSize(), ToColor(0x000000));
    cursor_.y = 0;
  }else {
    Print("command not found: ");
    Print(command);
    Print("\n");
  }
}
Message MakeLayerMessage(uint64_t task_id, unsigned int layer_id, LayerOperation op, Rectangle<int> area) {
  Message msg{Message::kLayer, task_id};
  msg.arg.layer.layer_id = layer_id;
  msg.arg.layer.op = op;
  msg.arg.layer.x = area.pos.x;
  msg.arg.layer.y = area.pos.y;
  msg.arg.layer.w = area.size.x;
  msg.arg.layer.h = area.size.y;

  return msg;
}

void TaskTerminal(uint64_t task_id, int64_t data) {
  __asm__("cli");
  Task& task = task_manager->CurrentTask();
  Terminal* terminal = new Terminal;
  layer_manager->Move(terminal->LayerID(), {100, 200});
  active_layer->Activate(terminal->LayerID());
  layer_task_map->insert(std::make_pair(terminal->LayerID(), task_id));
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
        {
          const auto area = terminal->BlinkCursor();
          Message msg = MakeLayerMessage(
            task_id, 
            terminal->LayerID(), 
            LayerOperation::DrawArea,
            area
          );

          __asm__("cli");
          task_manager->SendMessage(1, msg);
          __asm__("sti");
        }
        break;
      case Message::kKeyPush:
        {
          const auto area = terminal->InputKey(
            msg->arg.keyboard.modifier,
            msg->arg.keyboard.keycode,
            msg->arg.keyboard.ascii
          );

          Message msg = MakeLayerMessage(
            task_id,
            terminal->LayerID(),
            LayerOperation::DrawArea,
            area
          );

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