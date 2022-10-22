#include <cstdint>
#include <array>
#include "interrupt.hpp"

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