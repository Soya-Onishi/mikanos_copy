#include <array>

#include "segment.hpp"
#include "memory_manager.hpp"
#include "interrupt.hpp"
#include "asmfunc.h"
#include "logger.hpp"
#include "error.hpp"

namespace {
  std::array<SegmentDescriptor, 7> gdt;  
  std::array<uint32_t, 26> tss;

  static_assert((kTSS >> 3) + 1 < gdt.size());
}

void set_code_segment(
  SegmentDescriptor& desc,
  DescriptorType type,
  unsigned int descriptor_privilege_level,
  uint32_t base,
  uint32_t limit
) {
  desc.data = 0;

  desc.bits.base_low    =  base        & 0xFFFFu;
  desc.bits.base_middle = (base >> 16) & 0xFFu;
  desc.bits.base_high   = (base >> 24) & 0xFFu;

  desc.bits.limit_low  =  limit        & 0xFFFFu;
  desc.bits.limit_high = (limit >> 16) & 0x000Fu;

  desc.bits.type = type;
  desc.bits.system_segment = 1;
  desc.bits.descriptor_privilege_level = descriptor_privilege_level;
  desc.bits.present = 1;
  desc.bits.available = 0;
  desc.bits.long_mode = 1;
  desc.bits.default_operation_size = 0;

  // long-modeではlimitの値が無視されるため、ここの値に意味はなし。
  desc.bits.granularity = 1;
}

void set_data_segment(
  SegmentDescriptor& desc,
  DescriptorType type,
  unsigned int descriptor_privilege_level,
  uint32_t base,
  uint32_t limit
) {
  set_code_segment(desc, type, descriptor_privilege_level, base, limit);
  desc.bits.long_mode = 0;
  desc.bits.default_operation_size = 1; // 32bitスタックセグメント
}

void set_system_segment(
  SegmentDescriptor& desc,
  DescriptorType type,
  unsigned int descriptor_privilege_level,
  uint32_t base,
  uint32_t limit
) {
  set_code_segment(desc, type, descriptor_privilege_level, base, limit);
  desc.bits.system_segment = 0;
  desc.bits.long_mode = 0;
}

void set_TSS(int index, uint64_t value) {
  tss[index] = value & 0xFFFF'FFFF;
  tss[index + 1] = value >> 32;
}

uint64_t allocate_stack_area(int num_4kframes) {
  auto [stk, err] = memory_manager->Allocate(num_4kframes);
  if(err) {
    Log(kError, "failed to allocate stack area: %s\n", err.Name());
    exit(1);
  }

  return reinterpret_cast<uint64_t>(stk.Frame()) + num_4kframes * 4096;
}

void InitializeTSS() {
  set_TSS(1, allocate_stack_area(8));
  set_TSS(7 + 2 * kISTForTimer, allocate_stack_area(8));

  uint64_t  tss_addr = reinterpret_cast<uint64_t>(&tss[0]);
  set_system_segment(gdt[kTSS >> 3], DescriptorType::kTSSAvailable, 0, tss_addr & 0xFFFF'FFFF, sizeof(tss) - 1);
  gdt[(kTSS >> 3) + 1].data = tss_addr >> 32;

  LoadTR(kTSS);
}

void setup_segments() {
  gdt[0].data = 0;

  // long-modeではbaseとlimitは使用されないので、0と0xFFFFFuの適当な値にしている。
  set_code_segment(gdt[1], DescriptorType::kExecuteRead, 0, 0, 0xFFFFFu);
  set_data_segment(gdt[2], DescriptorType::kReadWrite, 0, 0, 0xFFFFFu);
  set_data_segment(gdt[3], DescriptorType::kReadWrite, 3, 0, 0xFFFFFu);
  set_code_segment(gdt[4], DescriptorType::kExecuteRead, 3, 0, 0xFFFFFu);
  LoadGDT(sizeof(gdt) - 1, reinterpret_cast<uint64_t>(&gdt[0]));
}

void InitializeSegment() {
  setup_segments();
  
  const uint16_t kernel_cs = 1 << 3;
  const uint64_t kernel_ss = 2 << 3;

  SetDSAll(0);  
  SetCSSS(kernel_cs, kernel_ss);      
}