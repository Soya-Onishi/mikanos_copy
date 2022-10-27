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
void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
  layer_manager->MoveRelative(mouse_layer_id, {displacement_x, displacement_y});
  StartAPICTimer();
  layer_manager->Draw();
  auto elapsed = LAPICTimerElapsed();
  StopLAPICTimer();
  printk("MouseObserver: elapsed = %u\n", elapsed);
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
  console->SetWriter(bgwriter);  

  auto mouse_window = std::make_shared<Window>(kMouseCursorWidth, kMouseCursorHeight, shadow_format);
  mouse_window->SetTransparentColor(kMouseTransparentColor);
  DrawMouseCursor(mouse_window->Writer(), {0, 0});

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
  
  layer_manager->UpDown(bglayer_id, 0);
  layer_manager->UpDown(mouse_layer_id, 1);
  layer_manager->Draw();  

  while(true) {
    __asm__("cli");
    if(message_queue->Count() == 0) {
      __asm__("sti");
      __asm__("hlt");
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