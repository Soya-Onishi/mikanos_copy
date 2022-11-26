#include <array>

#include "segment.hpp"
#include "asmfunc.h"

namespace {
  std::array<SegmentDescriptor, 5> gdt;  
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

void setup_segments() {
  gdt[0].data = 0;

  // long-modeではbaseとlimitは使用されないので、0と0xFFFFFuの適当な値にしている。
  set_code_segment(gdt[1], DescriptorType::kExecuteRead, 0, 0, 0xFFFFFu);
  set_data_segment(gdt[2], DescriptorType::kReadWrite, 0, 0, 0xFFFFFu);
  set_code_segment(gdt[3], DescriptorType::kExecuteRead, 3, 0, 0xFFFFFu);
  set_data_segment(gdt[4], DescriptorType::kReadWrite, 3, 0, 0xFFFFFu);
  LoadGDT(sizeof(gdt) - 1, reinterpret_cast<uint64_t>(&gdt[0]));
}

void InitializeSegment() {
  setup_segments();
  
  const uint16_t kernel_cs = 1 << 3;
  const uint64_t kernel_ss = 2 << 3;
  kKernelCS = kernel_cs;
  kKernelSS = kernel_ss;

  SetDSAll(0);  
  SetCSSS(kernel_cs, kernel_ss);      
}