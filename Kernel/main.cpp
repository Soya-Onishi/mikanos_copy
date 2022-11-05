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

  __asm__("sti");

  InitializeLayer();
  auto main_window_layer_id = InitializeMainWindow();    
  InitializeMouse();
  layer_manager->Draw({{0, 0}, ScreenSize()});  
  InitializeKeyboard(*message_queue);

  unsigned int count = 0;
  char counter_str[128];

  auto main_window_writer = layer_manager->GetLayer(main_window_layer_id).GetWindow()->Writer();
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
      __asm__("hlt");
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
        printk("Timer: timeout = %lu, value = %d\n", msg.arg.timer.timeout, msg.arg.timer.value);
        if(msg.arg.timer.value > 0) {
          timer_manager->AddTimer(Timer{msg.arg.timer.timeout + 100, msg.arg.timer.value + 1});
        }
        break;
      case Message::kKeyPush:
        if(msg.arg.keyboard.ascii != 0) {
          printk("%c", msg.arg.keyboard.ascii);
        }
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