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

struct TaskContext {
  uint64_t cr3, rip, rflags, reserved1;
  uint64_t cs, ss, fs, gs;
  uint64_t rax, rbx, rcx, rdx, rdi, rsi, rsp, rbp;
  uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
  std::array<uint8_t, 512> fxsave_area;
} __attribute__((packed));

alignas(16) TaskContext task_a_ctx, task_b_ctx;

int printk(const char* fmt, ...);

std::deque<Message>* message_queue;

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

  return task_b_window_layer_id;
}

void TaskB(int task_id, int data, int layer_id) {
  printk("TaskB: task_id=%d, data=%d\n", task_id, data);
  
  char str[128];
  int count = 0;
  const auto window = layer_manager->GetLayer(layer_id).GetWindow();
  while(true) {
    count++;
    sprintf(str, "%10d", count);
    FillRectangle(*window->Writer(), {24, 28}, {8 * 10, 16}, {0xC6, 0xC6, 0xC6});
    WriteString(*window->Writer(), {24, 28}, str, {0, 0, 0});
    layer_manager->Draw(layer_id);

    SwitchContext(&task_a_ctx, &task_b_ctx);
  }
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
  
  ::message_queue = new std::deque<Message>(32);
  InitializeInterrupt(::message_queue);
  InitializePCI();
  usb::xhci::Initialize();    

  acpi::Initialize(acpi_table);
  InitializeAPICTimer(*message_queue);
  const int kTextboxCursorTime = 1;
  const int kTimer05Sec = static_cast<int>(kTimerFreq * 0.5);
  bool text_cursor_visible = false;  
  timer_manager->AddTimer(Timer{kTimer05Sec, kTextboxCursorTime});

  __asm__("sti");  

  InitializeLayer();
  auto main_window_layer_id = InitializeMainWindow();    
  auto text_window_layer_id = InitializeTextWindow();
  auto task_b_window_layer_id = InitializeTaskBWindow();
  InitializeMouse();
  layer_manager->Draw({{0, 0}, ScreenSize()});  
  InitializeKeyboard(*message_queue);

  unsigned int count = 0;
  char counter_str[128];

  auto main_window_writer = layer_manager->GetLayer(main_window_layer_id).GetWindow()->Writer();
  auto text_window_writer = layer_manager->GetLayer(text_window_layer_id).GetWindow()->Writer();

  std::vector<uint64_t> task_b_stack(1024);
  uint64_t task_b_stack_end = reinterpret_cast<uint64_t>(&task_b_stack[1024]);

  // タスクBのコンテキストの初期化
  memset(&task_b_ctx, 0, sizeof(task_b_ctx));
  task_b_ctx.rip = reinterpret_cast<uint64_t>(TaskB);
  task_b_ctx.rdi = 1;
  task_b_ctx.rsi = 42;
  task_b_ctx.rdx = task_b_window_layer_id;
  
  task_b_ctx.cr3 = GetCR3();
  task_b_ctx.rflags = 0x202;
  task_b_ctx.cs = kKernelCS;
  task_b_ctx.ss = kKernelSS;
  task_b_ctx.rsp = (task_b_stack_end & ~0xFlu) - 8;

  *reinterpret_cast<uint32_t*>(&task_b_ctx.fxsave_area[24]) = 0x1F80;

  while(true) {
    __asm__("cli");
    const auto tick = timer_manager->CurrentTick();
    __asm__("sti");
    
    sprintf(counter_str, "0x%08X", tick);    
    FillRectangle(*main_window_writer, {24, 28}, {8 * 10, 16}, ToColor(0xC6C6C6));
    WriteString(*main_window_writer, {24, 28}, counter_str, ToColor(0x000000));
    layer_manager->Draw(main_window_layer_id);

    __asm__("cli");
    if(message_queue->size() == 0) {
      __asm__("sti");      
      SwitchContext(&task_b_ctx, &task_a_ctx);
      continue;
    }
    
    Message msg = message_queue->front();
    message_queue->pop_front();    

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