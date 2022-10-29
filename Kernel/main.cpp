#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <numeric>
#include <vector>
#include <array>

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

struct Message {
  enum Type {
    kInterruptXHCI,
  } type;
};

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
char console_buf[sizeof(Console)];
char message_queue_buf[sizeof(ArrayQueue<Message>)];
char memory_manager_buf[sizeof(BitmapMemoryManager)];

Console* console;
PixelWriter* pixel_writer;
usb::xhci::Controller* xhc;
ArrayQueue<Message>* message_queue;
std::array<Message, 32> queue_buffer;
BitmapMemoryManager* memory_manager;

alignas(16) uint8_t kernel_main_stack[1024 * 1024];

unsigned int mouse_layer_id;
void MouseObserver(uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
  static unsigned int mouse_drag_layer_id = 0;
  static uint8_t previous_buttons = 0;

  const auto layer = layer_manager->GetLayer(mouse_layer_id);
  const auto window = layer.GetWindow();
  auto width = window->Width();
  auto height = window->Height();
  auto old_pos = layer.GetPosition();  
  auto diff_pos = Vector2D<int> { 
    displacement_x, 
    displacement_y 
  };  
  auto tmp_pos = old_pos + diff_pos;
  auto new_pos = Vector2D<int>{
    std::min(std::max(0, tmp_pos.x), pixel_writer->Width() - 1),
    std::min(std::max(0, tmp_pos.y), pixel_writer->Height() - 1)
  };  
  
  layer_manager->Move(mouse_layer_id, new_pos);    

  const bool previous_left_pressed = (previous_buttons & 0x01);
  const bool left_pressed = (buttons & 0x01);
  
  if(!previous_left_pressed && left_pressed) {
    auto layer = layer_manager->FindLayerByPosition(old_pos, mouse_layer_id);

    if(layer && layer->IsDraggable()) {
      mouse_drag_layer_id = layer->ID();
    }
  } else if(previous_left_pressed && left_pressed) {
    if(mouse_drag_layer_id > 0) {
      layer_manager->MoveRelative(mouse_drag_layer_id, diff_pos);
    }
  } else if(previous_left_pressed && !left_pressed) {
    mouse_drag_layer_id = 0;
  }

  previous_buttons = buttons;
}

inline void halt() {
  while(1) {
    __asm__("hlt");
  }
}

__attribute__((interrupt))
void IntHandlerXHCI(InterruptFrame* frame) {
  Error err = message_queue->Push(Message{Message::kInterruptXHCI});
  if(err.Cause() != Error::kSuccess) {
    Log(kError, "Interrupt message queue is full");
  }  
  NotifyEndOfInterrupt();
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

void SwitchEhci2Xhci(const pci::Device& xhc_dev) {
  bool intel_ehc_exist = false;
  for (int i = 0; i < pci::num_device; ++i) {
    if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x20u) /* EHCI */ &&
        0x8086 == pci::ReadVendorId(pci::devices[i])) {
      intel_ehc_exist = true;
      break;
    }
  }
  if (!intel_ehc_exist) {
    return;
  }

  uint32_t superspeed_ports = pci::ReadConfReg(xhc_dev, 0xdc); // USB3PRM
  pci::WriteConfReg(xhc_dev, 0xd8, superspeed_ports); // USB3_PSSEN
  uint32_t ehci2xhci_ports = pci::ReadConfReg(xhc_dev, 0xd4); // XUSB2PRM
  pci::WriteConfReg(xhc_dev, 0xd0, ehci2xhci_ports); // XUSB2PR
  Log(kDebug, "SwitchEhci2Xhci: SS = %02, xHCI = %02x\n",
      superspeed_ports, ehci2xhci_ports);
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

extern "C" void kernel_main_new_stack(
  const FrameBufferConfig& frame_buffer_config_ref,
  const MemoryMap& memmap_ref
) {    
  __asm__("cli");

  FrameBufferConfig frame_buffer_config(frame_buffer_config_ref);
  MemoryMap memmap(memmap_ref);    

  switch(frame_buffer_config.pixel_format) {
    case kPixelRGBResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf) RGBResv8BitPerColorPixelWriter(frame_buffer_config);
      break;
    case kPixelBGRResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf) BGRResv8BitPerColorPixelWriter(frame_buffer_config);
      break;
    default:
      halt();   
  }

  // DrawDesktop(*pixel_writer);

  SetLogLevel(kInfo);

  InitializeAPICTimer();

  console = new(console_buf) Console(kDesktopFGColor, kDesktopBGColor);  
  console->SetWriter(pixel_writer);
  console->Clear();  

  printk("Setting Segment Register Start...\n");
  
  setup_segments();

  const uint16_t kernel_cs = 1 << 3;
  const uint64_t kernel_ss = 2 << 3;
  SetDSAll(0);  
  SetCSSS(kernel_cs, kernel_ss);  
  printk("Setting Segment Register Done...\n");  
  setup_identity_pagetable();

  ::memory_manager = new(memory_manager_buf) BitmapMemoryManager;
  const auto memory_map_base = reinterpret_cast<uintptr_t>(memmap.buffer);
  uintptr_t available_end = 0;
  for(
    uintptr_t iter = memory_map_base;
    iter < memory_map_base + memmap.map_size;
    iter += memmap.descriptor_size
  ) {
    auto desc = reinterpret_cast<const MemoryDescriptor*>(iter);
    if(available_end < desc->physical_start) {
      memory_manager->MarkAllocated(
        FrameID{ available_end / kBytesPerFrame },
        (desc->physical_start - available_end) / kBytesPerFrame
      );
    }

    const auto physical_end = desc->physical_start + desc->number_of_pages * kUEFIPageSize;

    // TODO: 要検証
    // IsAvailableがTrueでavailable_endを更新しているが、
    // Falseでもavailable_endを更新してもいいのでは？
    // Falseのときは更新していないが、
    // この場合次のループのときに同じ場所をMarkAllocatedすることにならないか
    if(IsAvailable(static_cast<MemoryType>(desc->type))) {
      available_end = physical_end;
    } else {
      memory_manager->MarkAllocated(
        FrameID{desc->physical_start / kBytesPerFrame},
        (desc->number_of_pages * kUEFIPageSize) / kBytesPerFrame
      );
    }
  }

  memory_manager->SetMemoryRange(FrameID{1}, FrameID{available_end / kBytesPerFrame});
  
  if(auto err = InitializeHeap(*memory_manager)) {
    Log(kError, "failed to allocate pages: %s at %s:%d\n", err.Name(), err.File(), err.Line());
    halt();
  }

  message_queue = new(message_queue_buf) ArrayQueue<Message>(queue_buffer);  

  auto err = pci::ScanAllBus();
  Log(kDebug, "ScanAllBus: %s\n", err.Name());

  for(int i = 0; i < pci::num_device; i++) {
    const auto& dev = pci::devices[i];
    auto vendor_id = pci::ReadVendorId(dev);
    auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);

    Log(kDebug, "%d.%d.%d: vend %04x, class %08x, head %02x\n",
      dev.bus, dev.device, dev.function,
      vendor_id, class_code, dev.header_type);
  }

  pci::Device* xhc_dev = nullptr;
  for(int i = 0; i < pci::num_device; i++) {
    if(pci::devices[i].class_code.Match(0x0Cu, 0x03u, 0x30u)) {
      xhc_dev = &pci::devices[i];

      if(0x8086 == pci::ReadVendorId(*xhc_dev)) {
        break;
      }
    }
  }

  if(!xhc_dev) {
    Log(kWarn, "xHC device is not found");
    halt();
  }

  const uint16_t cs = GetCS();
  // const uint64_t offset = reinterpret_cast<uint64_t>(IntHandlerXHCI);
  SetIDTEntry(
    idt[InterruptVector::kXHCI], 
    MakeIDTAttr(DescriptorType::kInterruptGate), 
    reinterpret_cast<uint64_t>(IntHandlerXHCI),
    cs
  );
  LoadIDT(sizeof(idt) - 1, reinterpret_cast<uint64_t>(&idt[0]));
 
  const uint8_t bsp_local_apic_id = *reinterpret_cast<const uint32_t*>(0xfee00020) >> 24;
  pci::ConfigureMSIFixedDestination(
    *xhc_dev,
    bsp_local_apic_id,
    pci::MSITriggerMode::kLevel,
    pci::MSIDeliveryMode::kFixed,
    InterruptVector::kXHCI,
    0
  );
  
  
  const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_dev, 0);
  const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf);

  usb::xhci::Controller xhc{xhc_mmio_base};

  if(0x8086 == pci::ReadVendorId(*xhc_dev)) {
    SwitchEhci2Xhci(*xhc_dev);
  } else {
    auto err = xhc.Initialize();
  }

  xhc.Run();

  ::xhc = &xhc;
  __asm__("sti");

  usb::HIDMouseDriver::default_observer = MouseObserver;

  for(int i = 1; i <= xhc.MaxPorts(); i++) {
    auto port = xhc.PortAt(i);

    if(port.IsConnected()) {
      if(auto err = ConfigurePort(xhc, port)) {
        continue;
      }
    }
  }
  
  const int kFrameWidth = frame_buffer_config.horizontal_resolution;
  const int kFrameHeight = frame_buffer_config.vertical_resolution;
  
  auto shadow_format = frame_buffer_config.pixel_format;
  auto bgwindow = std::make_shared<Window>(kFrameWidth, kFrameHeight, shadow_format);
  auto bgwriter = bgwindow->Writer();

  DrawDesktop(*bgwriter);  

  auto mouse_window = std::make_shared<Window>(kMouseCursorWidth, kMouseCursorHeight, shadow_format);
  mouse_window->SetTransparentColor(kMouseTransparentColor);
  DrawMouseCursor(mouse_window->Writer(), {0, 0});

  auto main_window = std::make_shared<Window>(160, 68, shadow_format);
  DrawWindow(*main_window->Writer(), "Hello Window");

  auto console_window = std::make_shared<Window>(
    Console::kColumns * 8, Console::kRows * 16, shadow_format
  );  
  console->SetWindow(console_window);

  FrameBuffer screen;
  if(auto err = screen.Initialize(frame_buffer_config)) {
    Log(kError, "failed to initialize frame buffer: %s at %s:%d", err.Name(), err.File(), err.Line());
  }

  layer_manager = new LayerManager;
  layer_manager->SetWriter(&screen);

  auto bglayer_id = layer_manager->NewLayer()
    .SetWindow(bgwindow)
    .Move({0, 0})
    .ID();
  mouse_layer_id = layer_manager->NewLayer()
    .SetWindow(mouse_window)
    .Move({200, 200})
    .ID();  
  auto main_window_layer_id = layer_manager->NewLayer()
    .SetWindow(main_window)
    .SetDraggable(true)
    .Move({300, 100})
    .ID();
  console->SetLayerID(layer_manager->NewLayer()
    .SetWindow(console_window)
    .Move({0, 0})
    .ID()
  );
  

  layer_manager->UpDown(bglayer_id, 0);  
  layer_manager->UpDown(console->LayerID(), 1);  
  layer_manager->UpDown(main_window_layer_id, 2);    
  layer_manager->UpDown(mouse_layer_id, 3);
  layer_manager->Draw(bglayer_id);

  unsigned int count = 0;
  char counter_str[128];

  while(true) {
    count++;
    sprintf(counter_str, "0x%08X", count);
    FillRectangle(*main_window->Writer(), {24, 28}, {8 * 10, 16}, ToColor(0xC6C6C6));
    WriteString(*main_window->Writer(), {24, 28}, counter_str, ToColor(0x000000));
    layer_manager->Draw(main_window_layer_id);

    __asm__("cli");
    if(message_queue->Count() == 0) {
      __asm__("sti");      
      continue;
    }
    
    Message msg = message_queue->Front();
    message_queue->Pop();    

    __asm__("sti");
    switch(msg.type) {
      case Message::kInterruptXHCI:
        while(xhc.PrimaryEventRing()->HasFront()) {
          if(auto err = ProcessEvent(xhc)) {
            Log(kError, "Error while Process Event: %s at %s:%d\n", err.Name(), err.File(), err.Line());
          }
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