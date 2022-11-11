#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <numeric>
#include <vector>
#include <array>
#include <deque>

#include "frame_buffer_config.hpp"
#include "memory_map.hpp"
#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"
#include "mouse.hpp"
#include "logger.hpp"
#include "interrupt.hpp"
#include "asmfunc.h"
#include "queue.hpp"
#include "segment.hpp"
#include "paging.hpp"
#include "memory_manager.hpp"
#include "layer.hpp"
#include "timer.hpp"
#include "acpi.hpp"
#include "keyboard.hpp"
#include "task.hpp"
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/classdriver/mouse.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/xhci/trb.hpp"

/*
void* operator new(size_t size, void* buf) {
  return buf;
}

void operator delete(void* obj) noexcept {}
*/

int printk(const char* fmt, ...);

alignas(16) uint8_t kernel_main_stack[1024 * 1024];

inline void halt() {
  while(1) {
    __asm__("hlt");
  }
}

int printk(const char* fmt, ...) {
  va_list ap;
  int result;
  char s[1024];

  va_start(ap, fmt);
  result = vsprintf(s, fmt, ap);
  va_end(ap);  

  console->PutString(s);

  return result;
}

void show_memory_map(const MemoryMap& memmap) {
  const std::array<MemoryType, 3> available_memory_types {
    MemoryType::kEfiBootServicesCode,
    MemoryType::kEfiBootServicesData,
    MemoryType::kEfiConventionalMemory,
  };

  printk("start memory map iteration: %d\n", memmap.map_size);    

  for(
    uintptr_t iter = reinterpret_cast<uintptr_t>(memmap.buffer);
    iter < reinterpret_cast<uintptr_t>(memmap.buffer) + memmap.map_size;
    iter += memmap.descriptor_size
  ) {        
    auto desc = reinterpret_cast<MemoryDescriptor*>(iter);
    for(int i = 0; i < available_memory_types.size(); i++) {
      if(desc->type == available_memory_types[i]) {
        printk(
          "type = %u, phys = %08lx - %08lx, pages = %lu, attr = %08lx\n",
          desc->type,
          desc->physical_start,
          desc->physical_start + desc->number_of_pages * 4096 - 1,
          desc->number_of_pages,
          desc->attribute
        );
        break;
      }
    }
  }
}

constexpr PixelColor ToColor(uint32_t c) {
  return {
    static_cast<uint8_t>((c >> 16) & 0xFF),
    static_cast<uint8_t>((c >>  8) & 0xFF),
    static_cast<uint8_t>((c      ) & 0xFF)
  };
}

void DrawWindow(PixelWriter& writer, const char* title) {
  auto fill_rect = [&writer](Vector2D<int> pos, Vector2D<int> size, uint32_t c) {
    FillRectangle(writer, pos, size, ToColor(c));
  };

  const auto win_w = writer.Width();
  const auto win_h = writer.Height();

  fill_rect({0, 0}, {win_w, win_h}, 0xC6C6C6);
  fill_rect({1, 1}, {win_w - 1, 20}, 0x000084);  
  WriteString(writer, {2, 4}, title, ToColor(0xFFFFFF));
  
  // Draw Close button
  auto button_base = Vector2D<int>{win_w - (16 + 2), 2};
  fill_rect(button_base, {16, 16}, 0xC6C6C6);
  for(int y = 2; y < (16 - 2); y++) {
    for(int x = 2; x < (16 - 2); x++) {
      if(x == y || (16 - x) == y) {
        writer.Write(button_base + Vector2D<int>{x, y}, ToColor(0x000000));
      }
    }
  }
}

unsigned int InitializeMainWindow() {
  auto main_window = std::make_shared<Window>(160, 52, screen_config.pixel_format);
  DrawWindow(*main_window->Writer(), "Hello Window");

  auto main_window_layer_id = layer_manager->NewLayer()
    .SetWindow(main_window)
    .SetDraggable(true)
    .Move({300, 100})
    .ID();

  layer_manager->UpDown(main_window_layer_id, std::numeric_limits<int>::max());

  return main_window_layer_id;
}

void DrawTextBox(PixelWriter& writer, Vector2D<int> pos, Vector2D<int> size) {
  auto fill_rect = 
    [&writer](Vector2D<int> pos, Vector2D<int> size, uint32_t c) {
      FillRectangle(writer, pos, size, ToColor(c));
    };

  fill_rect(pos + Vector2D<int>{1, 2}, size - Vector2D<int>{2, 2}, 0xFFFFFF);
  fill_rect(pos,                       {size.x, 1},                0x848484);
  fill_rect(pos, {1, size.y}, 0x848484);
  fill_rect(pos + Vector2D<int>{0, size.y}, {size.x, 1}, 0xc6c6c6);
  fill_rect(pos + Vector2D<int>{size.x, 0}, {1, size.y}, 0xc6c6c6);
}

std::shared_ptr<Window> text_window;
unsigned InitializeTextWindow() {
  const int win_w = 160;
  const int win_h = 52;

  text_window = std::make_shared<Window>(win_w, win_h, screen_config.pixel_format);
  DrawWindow(*text_window->Writer(), "Text Box Test");
  DrawTextBox(*text_window->Writer(), {4, 24}, {win_w - 8, win_h - 24 - 4});

  auto text_window_layer_id = layer_manager->NewLayer()
    .SetWindow(text_window)
    .SetDraggable(true)
    .Move({350, 200})
    .ID();

  layer_manager->UpDown(text_window_layer_id, std::numeric_limits<int>::max());

  return text_window_layer_id;
}

int text_window_index;
void DrawTextCursor(Window::WindowWriter& writer, bool visible) {
  const auto color = visible ? 0x000000 : 0xFFFFFF;
  const auto pos = Vector2D<int>{8 + 8 * text_window_index, 24 + 5};

  FillRectangle(writer, pos, {7, 15}, ToColor(color));
}

void InputTextWindow(Window& window, const unsigned int id, char c) {
  if(c == 0) {
    return;
  }

  auto pos = []() { return Vector2D<int>{8 + 8 * text_window_index, 24 + 6}; };
  const int max_chars = (text_window->Width() - 16) / 8 - 1;
  if(c == '\b' && text_window_index > 0) {
    DrawTextCursor(*window.Writer(), false);
    text_window_index--;
    FillRectangle(*window.Writer(), pos(), {8, 16}, ToColor(0xFFFFFF));
  } else if(c >= ' ' && text_window_index < max_chars) {
    DrawTextCursor(*window.Writer(), false);
    WriteAscii(*window.Writer(), pos(), c, ToColor(0x000000));
    text_window_index++;
  } 

  layer_manager->Draw(id);
}

unsigned int task_b_layer_id;
unsigned int InitializeTaskBWindow() {
  auto task_b_window = std::make_shared<Window> (
    160, 52, screen_config.pixel_format
  );
  DrawWindow(*task_b_window->Writer(), "TaskB Window");

  auto task_b_window_layer_id = layer_manager->NewLayer()
    .SetWindow(task_b_window)
    .SetDraggable(true)
    .Move({100, 100})
    .ID();

  layer_manager->UpDown(task_b_window_layer_id, std::numeric_limits<int>::max());
  task_b_layer_id = task_b_window_layer_id;

  return task_b_window_layer_id;
}

void TaskB(uint64_t task_id, int64_t data) {
  printk("TaskB: task_id=%d, data=%d\n", task_id, data);
  
  char str[128];
  int count = 0;
  auto window = layer_manager->GetLayer(task_b_layer_id).GetWindow();

  __asm__("cli");
  Task& task = task_manager->CurrentTask();
  __asm__("sti");

  while(true) {
    count++;
    sprintf(str, "%10d", count);
    FillRectangle(*window->Writer(), {24, 28}, {8 * 10, 16}, {0xC6, 0xC6, 0xC6});
    WriteString(*window->Writer(), {24, 28}, str, {0, 0, 0});

    Message msg{Message::kLayer, task_id};
    msg.arg.layer.layer_id = task_b_layer_id;
    msg.arg.layer.op = LayerOperation::Draw;

    __asm__("cli");
    task_manager->SendMessage(1, msg);
    __asm__("sti");

    while(true) {
      __asm__("cli");
      auto msg = task.ReceiveMessage();
      if(!msg) {
        task.Sleep();
        __asm__("sti");
        continue;
      }

      if(msg->type == Message::kLayerFinish) {
        break;
      }
    }
  }
}

void IdleTask(uint64_t task_id, int64_t data) {
  printk("TaskIdle: task_id=%lu, data=%lx\n", task_id, data);
  while(true) __asm__("hlt");
}

extern "C" void kernel_main_new_stack(
  const FrameBufferConfig& frame_buffer_config_ref,
  const MemoryMap& memmap_ref,
  const acpi::RSDP& acpi_table
) {    
  __asm__("cli");

  MemoryMap memmap(memmap_ref);    

  SetLogLevel(kInfo);

  InitializeGraphics(frame_buffer_config_ref);
  InitializeConsole();  
  InitializeSegment();
  InitializePagetable();
  InitializeMemoryManager(memmap);    
    
  InitializeInterrupt();
  InitializePCI();
  
  acpi::Initialize(acpi_table);
  InitializeAPICTimer();
  const int kTextboxCursorTime = 1;
  const int kTimer05Sec = static_cast<int>(kTimerFreq * 0.5);
  bool text_cursor_visible = false;  
  timer_manager->AddTimer(Timer{kTimer05Sec, kTextboxCursorTime});
  
  InitializeLayer();
  auto main_window_layer_id = InitializeMainWindow();    
  auto text_window_layer_id = InitializeTextWindow();
  auto task_b_window_layer_id = InitializeTaskBWindow();  
  layer_manager->Draw({{0, 0}, ScreenSize()});  
  
  unsigned int count = 0;
  char counter_str[128];

  auto main_window_writer = layer_manager->GetLayer(main_window_layer_id).GetWindow()->Writer();
  auto text_window_writer = layer_manager->GetLayer(text_window_layer_id).GetWindow()->Writer();
  
  InitializeTask();
  Task& main_task = task_manager->CurrentTask();
  auto task_b = task_manager->NewTask().InitContext(TaskB, 45).Wakeup();

  usb::xhci::Initialize();    
  InitializeMouse();
  InitializeKeyboard();

  __asm__("sti");  

  while(true) {
    __asm__("cli");
    const auto tick = timer_manager->CurrentTick();    
    __asm__("sti");
    
    sprintf(counter_str, "0x%08X", tick);    
    FillRectangle(*main_window_writer, {24, 28}, {8 * 10, 16}, ToColor(0xC6C6C6));
    WriteString(*main_window_writer, {24, 28}, counter_str, ToColor(0x000000));
    layer_manager->Draw(main_window_layer_id);
    
    __asm__("cli");
    auto msg_opt = main_task.ReceiveMessage();
    if(!msg_opt.has_value()) {
      main_task.Sleep();      
      __asm__("sti");
      continue;
    }
    
    auto msg = msg_opt.value();    
    __asm__("sti");
    switch(msg.type) {
      case Message::kInterruptXHCI:
        usb::xhci::ProcessEvents();        
        break;     
      case Message::kTimerTimeout:                
        if(msg.arg.timer.value == kTextboxCursorTime) {          
          __asm__("cli");          
          timer_manager->AddTimer(Timer{msg.arg.timer.timeout + kTimer05Sec, kTextboxCursorTime});
          __asm__("sti");
          text_cursor_visible = !text_cursor_visible;
          DrawTextCursor(*text_window_writer, text_cursor_visible);
          layer_manager->Draw(text_window_layer_id);
        }
        
        break;
      case Message::kKeyPush:
        InputTextWindow(*text_window, text_window_layer_id, msg.arg.keyboard.ascii);
        switch(msg.arg.keyboard.ascii) {
          case 's':
            printk("sleep taskB: %s\n", task_manager->Sleep(task_b.ID()).Name());
            break;
          case 'w':
            printk("wakeup taskB: %s\n", task_manager->Wakeup(task_b.ID()).Name());
            break;
          default:
            break;
        }
        break;
      case Message::kLayer:
        ProcessLayerMessage(msg);
        __asm__("cli");
        task_manager->SendMessage(msg.src_task, Message{Message::kLayerFinish});
        __asm__("sti");
        break;
      default:
        Log(kError, "Unknown message type: %d\n", msg.type);
        break;
    }
  }

  halt();
} 

extern "C" void __cxa_pure_virtual() {
  halt();
}