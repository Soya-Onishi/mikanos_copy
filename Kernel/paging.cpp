#include <cstdint>
#include <array>

#include "paging.hpp"
#include "memory_manager.hpp"
#include "asmfunc.h"
#include "error.hpp"
#include "logger.hpp"

namespace {
  const uint64_t kPageSize4K = 4096;
  const uint64_t kPageSize2M = 512 * kPageSize4K;
  const uint64_t kPageSize1G = 512 * kPageSize2M;

  alignas(kPageSize4K) std::array<uint64_t, 512> pml4_table;
  alignas(kPageSize4K) std::array<uint64_t, 512> pdp_table;
  alignas(kPageSize4K) std::array<std::array<uint64_t, 512>, kPageDirectoryCount> page_directory;
}

void InitializePagetable() {
  pml4_table[0] = reinterpret_cast<uint64_t>(&pdp_table[0]) | 0x03;
  for(int i_pdpt = 0; i_pdpt < page_directory.size(); i_pdpt++) {
    pdp_table[i_pdpt] = reinterpret_cast<uint64_t>(&page_directory[i_pdpt]) | 0x03;

    for(int i_pd = 0; i_pd < 512; i_pd++) {
      page_directory[i_pdpt][i_pd] = i_pdpt * kPageSize1G + i_pd * kPageSize2M | 0x83;
    }
  }  

  SetCR3(reinterpret_cast<uint64_t>(&pml4_table[0]));
}

WithError<PageMapEntry*> NewPageMap() {
  auto frame = memory_manager->Allocate(1); 
  if(frame.error) {
    return { nullptr, frame.error };
  } 

  auto e = reinterpret_cast<PageMapEntry*>(frame.value.Frame());
  memset(e, 0, sizeof(uint64_t) * 512);
  return { e, MAKE_ERROR(Error::kSuccess) };
}

WithError<PageMapEntry*> SetNewPageMapIfNotPresent(PageMapEntry* entry) {
  if(entry->bits.present) {
    return { entry->Pointer(), MAKE_ERROR(Error::kSuccess) };
  }

  auto [ child_map, err ] = NewPageMap();
  if(err) {
    return { nullptr, err };
  }

  entry->SetPointer(child_map);
  entry->bits.present = 1;



  return { child_map, MAKE_ERROR(Error::kSuccess) };
}

WithError<size_t> SetupPageMap(PageMapEntry* table, int page_map_level, LinearAddress4Level addr, size_t num_4kpages) {
  while(num_4kpages > 0) {
    const auto entry_index = addr.Part(page_map_level);
    auto [ child_table, err ] = SetNewPageMapIfNotPresent(&table[entry_index]);
    Log(kInfo, "setting child page done map: %p, next: %p, present: %d\n", child_table, table[entry_index].Pointer(), table[entry_index].bits.present);
    if(err) {
      return { num_4kpages, err };
    }
    table[entry_index].bits.writable = 1;

    if(page_map_level == 1) {
      num_4kpages--;
    } else {
      auto [ num_remain_pages, err ] = SetupPageMap(child_table, page_map_level - 1, addr, num_4kpages);
      if(err) {
        return { num_4kpages, err };
      }

      num_4kpages = num_remain_pages;
    }

    if(entry_index == 511) {
      break;
    }

    addr.SetPart(page_map_level, entry_index + 1);
    for(int level = page_map_level - 1; level >= 1; level--) {
      addr.SetPart(level, 0);
    }
  }

  // TODO 現状だとPML4の末端に割当途中に到達してしまった場合も
  //      ここでkSuccessを返しているため、割当が成功してしまった判定になる？
  //      この実装で大丈夫かどうか判断する必要あり。 
  return { num_4kpages, MAKE_ERROR(Error::kSuccess) };
}

Error SetupPageMaps(LinearAddress4Level addr, size_t num_4kpages) {
  auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
  return SetupPageMap(pml4_table, 4, addr, num_4kpages).error;
}

Error CleanPageMap(PageMapEntry* table, int page_map_level) {
  for(int i = 0; i < 512; i++) {
    auto entry = table[i];
    if(!entry.bits.present) {
      continue;
    }

    if(page_map_level > 1) {
      if(auto err = CleanPageMap(entry.Pointer(), page_map_level - 1)) {
        return err;
      }
    }

    const auto entry_addr = reinterpret_cast<uintptr_t>(entry.Pointer());
    const FrameID map_frame{entry_addr / kBytesPerFrame};
    if(auto err = memory_manager->Free(map_frame, 1)) {
      return err;
    }

    table[i].data = 0;
  }

  return MAKE_ERROR(Error::kSuccess);
}

Error CleanPageMaps(LinearAddress4Level addr) {
  auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
  auto pdp_table = pml4_table[addr.parts.pml4].Pointer();
  pml4_table[addr.parts.pml4].data = 0;
  if(auto err = CleanPageMap(pdp_table, 3)) {
    return err;
  }

  const auto pdp_addr = reinterpret_cast<uintptr_t>(pdp_table);
  const FrameID pdp_frame{pdp_addr / kBytesPerFrame};
  return memory_manager->Free(pdp_frame, 1);
}