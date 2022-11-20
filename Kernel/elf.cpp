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

    const auto src = reinterpret_cast<uint8_t*>(ehdr) + phdr->p_offset;
    const auto dst = reinterpret_cast<uint8_t*>(phdr->p_vaddr);

    LinearAddress4Level dest_addr;
    dest_addr.value = phdr->p_vaddr;
    auto begin_offset = reinterpret_cast<uint64_t>(dst) & 0x0FFF;
    // 必要ページ数を計算するときはp_memszだけでなく
    // p_vaddrの開始アドレスとページの開始アドレスのズレ分も考慮する必要がある。
    // 例) p_memsz = 4000, p_vaddr = 1000のとき、p_memszだけを考慮すると1ページの確保になるが、
    //     実際にはp_vaddr分ずれるのでアドレスでは4999まで使用することになる。
    //     そのため、2ページ分を確保して4999まで使えるようにしてあげる必要がある。
    const auto num_4kpages = (begin_offset + phdr->p_memsz + 4095) / 4096;
    if(auto err = SetupPageMaps(dest_addr, num_4kpages)) {
      return err;
    }

    auto table = reinterpret_cast<PageMapEntry*>(GetCR3());
    for(int level = 4; level >= 1; level--) {
      table = reinterpret_cast<PageMapEntry*>(table[dest_addr.Part(level)].Pointer());
    } 
 
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

  return MAKE_ERROR(Error::kSuccess);
}