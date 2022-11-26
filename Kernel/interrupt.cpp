#include <cstdint>
#include <array>
#include <deque>

#include "interrupt.hpp"
#include "message.hpp"
#include "asmfunc.h"
#include "timer.hpp"
#include "task.hpp"
#include "asmfunc.h"

#include "pci.hpp"
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/classdriver/mouse.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/xhci/trb.hpp"

void SetIDTEntry(
  InterruptDescriptor& desc,
  InterruptDescriptorAttribute attr,
  uint64_t offset,
  uint16_t segment_selector
) {
  desc.attr = attr;
  desc.offset_low = offset & 0xffffu;
  desc.offset_middle = (offset >> 16) & 0xffffu;
  desc.offset_high = offset >> 32;
  desc.segment_selector = segment_selector;
}

InterruptDescriptorAttribute MakeIDTAttr(
  DescriptorType type,
  uint8_t descriptor_privilege_level,
  bool present,
  uint8_t interrupt_stack_table
) {
  InterruptDescriptorAttribute attr {};

  attr.bits.type = type;
  attr.bits.descriptor_privilege_level = descriptor_privilege_level;
  attr.bits.present = present;
  attr.bits.interrupt_stack_table = interrupt_stack_table;

  Log(kInfo, "data: %04x, size: %d\n", attr.data, sizeof(InterruptDescriptor));

  return attr;
}

void NotifyEndOfInterrupt() {
  volatile auto end_of_interrupt = reinterpret_cast<uint32_t*>(0xfee000b0);
  *end_of_interrupt = 0;
}

__attribute__((interrupt))
void IntHandlerXHCI(InterruptFrame* frame) {
  task_manager->SendMessage(1, Message{Message::kInterruptXHCI});  
  NotifyEndOfInterrupt();
}

void InitializeInterrupt() {  
  const uint16_t cs = GetCS();
  // const uint64_t offset = reinterpret_cast<uint64_t>(IntHandlerXHCI);
  SetIDTEntry(
    idt[InterruptVector::kXHCI], 
    MakeIDTAttr(DescriptorType::kInterruptGate), 
    reinterpret_cast<uint64_t>(IntHandlerXHCI),
    cs
  );  
  SetIDTEntry(
    idt[InterruptVector::kLAPICTimer],
    MakeIDTAttr(DescriptorType::kInterruptGate),
    reinterpret_cast<uint64_t>(IntHandlerLAPICTimer),
    cs
  );
  LoadIDT(sizeof(idt) - 1, reinterpret_cast<uint64_t>(&idt[0]));
}