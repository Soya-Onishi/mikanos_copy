#include <vector>
#include <memory>

#include "terminal.hpp"
#include "window.hpp"
#include "layer.hpp"
#include "task.hpp"
#include "font.hpp"
#include "logger.hpp"
#include "pci.hpp"
#include "fat.hpp"
#include "elf.hpp"
#include "paging.hpp"
#include "error.hpp"
#include "asmfunc.h"

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

  cmd_history_.resize(8);
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

      if(linebuf_index_ > 0) {
        cmd_history_.pop_back();
        cmd_history_.push_front(linebuf_);
      }
      
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
      if(keycode == 0x51) {
        draw_area = HistoryUpDown(-1);
      } else if (keycode == 0x52) {
        draw_area = HistoryUpDown(1);
      } else if(ascii != 0 && cursor_.x < kColumns - 1 && linebuf_index_ < kLineMax - 1) {
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

void Terminal::Print(const char c) {
  auto newline = [this]() {
    cursor_.x = 0;
    if(cursor_.y < kRows - 1) {
      ++cursor_.y;
    } else {
      this->Scroll_OneLine();
    }
  };

  if(c == '\n') {
    newline();
  } else {
    WriteAscii(*window_->InnerWriter(), CalcCursorPos(), c, ToColor(0xFFFFFF));
    if(cursor_.x == kColumns - 1) {
      newline();
    } else {
      cursor_.x++;
    }
  }  
}

void Terminal::Print(const char* s) {
  DrawCursor(false);

  while(*s) {
    Print(*s);
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
  } else if(strcmp(command, "lspci") == 0) {
    char s[64];
    for (int i = 0; i < pci::num_device; i++) {
      const auto& dev = pci::devices[i];
      auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
      sprintf(s, "%02x:%02x.%d vend=%04x head=%02x class=%02x.%02x.%02x\n",
        dev.bus, dev.device, dev.function, vendor_id, dev.header_type,
        dev.class_code.base, dev.class_code.sub, dev.class_code.interface
      );
      Print(s);
    }
  } else if(strcmp(command, "ls") == 0) {
    auto root_dir_entries = fat::GetSectorByCluster<fat::DirectoryEntry>(fat::boot_volume_image->root_cluster);
    auto entries_per_cluster = fat::boot_volume_image->bytes_per_sector / sizeof(fat::DirectoryEntry) * fat::boot_volume_image->sectors_per_cluster;
    char base[9];
    char ext[4];

    char s[64];
    for(int i = 0; i < entries_per_cluster; i++) {
      fat::ReadName(root_dir_entries[i], base, ext);
      uint8_t special_code = static_cast<uint8_t>(base[0]);
      if(special_code == 0x00) {
        break;
      } else if(special_code == 0xE5) {
        continue;
      } else if(root_dir_entries[i].attr == fat::Attribute::kLongName) {
        continue;
      }

      if(ext[0]) {
        sprintf(s, "%s.%s\n", base, ext);
      } else {
        sprintf(s, "%s\n", base);
      }

      Print(s);
    }
  } else if(strcmp(command, "cat") == 0) {
    char s[64];
    auto file_entry = fat::FindFile(first_arg);

    if(!file_entry) {
      sprintf(s, "no such file: %s\n", first_arg);
      Print(s);
    } else {
      auto cluster = file_entry->FirstCluster();
      auto remain_bytes = file_entry->file_size;

      DrawCursor(false);
      
      while(cluster != 0 && cluster != fat::kEndOfClusterChain) {
        char* p = fat::GetSectorByCluster<char>(cluster);

        int i = 0;
        for(; i < fat::bytes_per_cluster && i < remain_bytes; i++) {
          Print(*p);
          p++;
        }

        remain_bytes -= i;
        cluster = fat::NextCluster(cluster);
      }
      

      DrawCursor(true);
    }
  } else if(command[0] != 0) {
    auto file_entry = fat::FindFile(command);
    if(!file_entry) {
      Print("command not found: ");
      Print(command);
      Print("\n");
    } else {
      if(auto err = ExecuteFile(*file_entry, command, first_arg)) {
        char s[128];
        sprintf(s, "failed to execute file: %s %s:%d\n", err.Name(), err.File(), err.Line());
        Print(s);
      }
    }    
  }
}

WithError<int> MakeArgVector(char* command, char* first_arg, char** argv, int argv_len, char* argbuf, int argbuf_len) {
  int argc = 0;
  int argbuf_index = 0;
  
  auto push_to_argv = [&](const char* s) {
    if(argc >= argv_len || argbuf_index >= argbuf_len) {
      return MAKE_ERROR(Error::kFull);
    }

    argv[argc] = &argbuf[argbuf_index];
    strcpy(&argbuf[argbuf_index], s);

    argbuf_index += strlen(s) + 1;
    argc++;

    return MAKE_ERROR(Error::kSuccess);
  };

  if(auto err = push_to_argv(command)) {
    return { argc, err };
  }

  char* p = first_arg;
  while(true) {
    while(isspace(p[0])) {
      p++;
    }

    if(p[0] == 0) {
      break;
    } 

    if(auto err = push_to_argv(p)) {
      return { argc, err };
    }

    while(p[0] != 0 && !isspace(p[0])) {
      p++;
    }

    if(p[0] == 0) {
      break;
    }

    p[0] = 0;
    p++;
  }

  return { argc, MAKE_ERROR(Error::kSuccess) };
}

Error Terminal::ExecuteFile(const fat::DirectoryEntry& file_entry, char* command, char* first_arg) {
  std::vector<uint8_t> file_buf(file_entry.file_size);
  if(file_entry.file_size != fat::LoadFile(&file_buf[0], file_entry.file_size, file_entry)) {
    return MAKE_ERROR(Error::kBufferTooSmall);
  }

  auto elf_header = reinterpret_cast<Elf64_Ehdr*>(&file_buf[0]);
  if(memcmp(elf_header->e_ident, "\x7f" "ELF", 4) != 0) {
    using Func = void();
    auto f = reinterpret_cast<Func*>(&file_buf[0]);
    f();
    return MAKE_ERROR(Error::kSuccess);
  }

  if(auto err = LoadElf(elf_header)) {
    return err;
  }

  LinearAddress4Level stack_frame_addr{ 0xFFFFFFFFFFFFE000 };
  if(auto err = SetupPageMaps(stack_frame_addr, 1)) {
    return err;
  }

  LinearAddress4Level args_frame_addr{ 0xFFFF'FFFF'FFFF'F000 };
  if(auto err = SetupPageMaps(args_frame_addr, 1)) {
    return err;
  }
  auto argv = reinterpret_cast<char**>(args_frame_addr.value);
  int argv_len = 32;
  auto argbuf = reinterpret_cast<char*>(args_frame_addr.value + sizeof(char**) * argv_len);
  int argbuf_len = 4096 - sizeof(char**) * argv_len;
  auto [argc, err] = MakeArgVector(command, first_arg, argv, argv_len, argbuf, argbuf_len);
  if(err) {
    return err;
  }

  auto entry_addr = elf_header->e_entry;
  CallApp(argc, argv, 4 << 3 | 3, 3 << 3 | 3, entry_addr, stack_frame_addr.value + 4096 - 8);

  /*
  char s[64];
  sprintf(s, "app exited. ret = %d\n", ret);
  Print(s);
  */

  const auto first_addr = GetFirstLoadAddress(elf_header);
  if(auto err = CleanPageMaps(LinearAddress4Level{first_addr})) {
    return err;
  }

  return MAKE_ERROR(Error::kSuccess);
}

Rectangle<int> Terminal::HistoryUpDown(int direction) {
  if(direction == -1 && cmd_history_index_ >= 0) {
    cmd_history_index_--;
  } else if(direction == 1 && cmd_history_index_ + 1 < cmd_history_.size()) {
    cmd_history_index_++;
  }

  cursor_.x = 1;
  const auto first_pos = CalcCursorPos();

  Rectangle<int> draw_area{first_pos, {(kColumns - 1) * 8, 16}};
  FillRectangle(*window_->InnerWriter(), draw_area.pos, draw_area.size, ToColor(0x000000));

  const char* history = "";
  if(cmd_history_index_ >= 0) {
    history = &cmd_history_[cmd_history_index_][0];
  }

  strcpy(&linebuf_[0], history);
  linebuf_index_ = strlen(history);

  WriteString(*window_->InnerWriter(), first_pos, history, ToColor(0xFFFFFF));
  cursor_.x = linebuf_index_ + 1;

  draw_area.pos = draw_area.pos + ToplevelWindow::kTopLeftMargin;

  return draw_area; 
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

    __asm__("sti");
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