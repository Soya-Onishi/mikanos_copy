#pragma once

#include <cstdint>
#include <array>
#include <deque>

#include "message.hpp"
#include "logger.hpp"
#include "x86_descriptor.hpp"

class InterruptVector {
  public:
    enum Number {
      kXHCI = 0x40,
      kLAPICTimer = 0x41,
    };
};

union InterruptDescriptorAttribute {
  uint16_t data;
  struct {
    uint16_t interrupt_stack_table: 3;
    uint16_t : 5;
    DescriptorType type: 4;
    uint16_t : 1;
    uint16_t descriptor_privilege_level : 2;
    uint16_t present: 1;
  } __attribute__((packed)) bits;
} __attribute__((packed));

struct InterruptDescriptor {
  uint16_t offset_low;
  uint16_t segment_selector;
  InterruptDescriptorAttribute attr;
  uint16_t offset_middle;
  uint32_t offset_high;
  uint32_t reserved;
} __attribute__((packed));

inline std::array<InterruptDescriptor, 256> idt;

void SetIDTEntry(
  InterruptDescriptor& desc, 
  InterruptDescriptorAttribute attr, 
  uint64_t offset, 
  uint16_t segment_selector
);

void NotifyEndOfInterrupt();

InterruptDescriptorAttribute MakeIDTAttr(
  DescriptorType type,
  uint8_t descriptor_privilege_level = 0,
  bool present = true,
  uint8_t interrupt_stack_table = 0
);

struct InterruptFrame {
  uint64_t rip;
  uint64_t cs;
  uint64_t rflags;
  uint64_t rsp;
  uint64_t ss;
};

void InitializeInterrupt();

const int kISTForTimer = 1;