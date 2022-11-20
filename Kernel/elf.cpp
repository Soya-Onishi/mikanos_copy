#include <cstdint>

#include "elf.hpp"
#include "paging.hpp"
#include "error.hpp"
#include "logger.hpp"
#include "asmfunc.h"

Elf64_Phdr* GetProgramHeader(Elf64_Ehdr* ehdr) {
  auto ehdr_head = reinterpret_cast<uintptr_t>(ehdr);
  return reinterpret_cast<Elf64_Phdr*>(ehdr_head + ehdr->e_phoff);
}

Error CopyLoadSegments(Elf64_Ehdr* ehdr) {
  auto phdr_head = GetProgramHeader(ehdr);
  for(int i = 0; i < ehdr->e_phnum; i++) {
    auto phdr_ptr = reinterpret_cast<uint8_t*>(phdr_head) + ehdr->e_phentsize * i;
    auto phdr = reinterpret_cast<Elf64_Phdr*>(phdr_ptr); 
    if(phdr->p_type != PT_LOAD) {
      continue; 
    }

    LinearAddress4Level dest_addr;
    dest_addr.value = phdr->p_vaddr;
    const auto num_4kpages = (phdr[i].p_memsz + 4095) / 4096;
    if(auto err = SetupPageMaps(dest_addr, num_4kpages)) {
      return err;
    }

    const auto src = reinterpret_cast<uint8_t*>(ehdr) + phdr->p_offset;
    const auto dst = reinterpret_cast<uint8_t*>(phdr->p_vaddr);
    Log(kInfo, "src: %p, dst: %p, filesz: %d, memsz: %d\n", src, dst, phdr->p_filesz, phdr->p_memsz);
    auto table = reinterpret_cast<PageMapEntry*>(GetCR3());
    for(int i = 4; i >= 1; i--) {
      Log(kInfo, "present: %d, next: %p, idx: %d\n", table->bits.present, table[dest_addr.Part(i)].Pointer(), dest_addr.Part(i));
      table = reinterpret_cast<PageMapEntry*>(table[dest_addr.Part(i)].Pointer());
    } 
    while(1)__asm__("hlt");
    memcpy(dst, src, phdr->p_filesz);
    memset(dst + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
  }

  return MAKE_ERROR(Error::kSuccess);
}

uintptr_t GetFirstLoadAddress(Elf64_Ehdr* ehdr) {
  auto phdr_head = GetProgramHeader(ehdr);
  for(int i = 0; i < ehdr->e_phnum; i++) {
    auto phdr_ptr = reinterpret_cast<uint8_t*>(phdr_head);
    auto phdr = reinterpret_cast<Elf64_Phdr*>(phdr_ptr + ehdr->e_phentsize * i);
    if(phdr->p_type != PT_LOAD) {
      continue;
    }

    return reinterpret_cast<uintptr_t>(phdr->p_vaddr);
  }

  return 0;
}

Error LoadElf(Elf64_Ehdr* ehdr) {
  if(ehdr->e_type != ET_EXEC) {
    return MAKE_ERROR(Error::kInvalidFormat);
  }

  const auto addr_first = GetFirstLoadAddress(ehdr);
  if(addr_first < 0xFFFF'8000'0000'0000) {
    return MAKE_ERROR(Error::kInvalidFormat);
  }

  if(auto err = CopyLoadSegments(ehdr)) {
    return err;
  }

  Log(kInfo, "CopyLoadSegment done\n");
  while(1) __asm__("hlt");
  return MAKE_ERROR(Error::kSuccess);
}