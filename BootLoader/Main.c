#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DiskIo2.h>
#include <Protocol/BlockIo.h>
#include <Guid/FileInfo.h>
#include <Guid/Acpi.h>
#include "elf.hpp"
#include "../Kernel/frame_buffer_config.hpp"
#include "../Kernel/memory_map.hpp"
#include <stdalign.h>

EFI_STATUS GetMemoryMap(struct MemoryMap* map);
EFI_STATUS SaveMemoryMap(struct MemoryMap* map, EFI_FILE_PROTOCOL* file);
EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL** root);
EFI_STATUS OpenGOP(EFI_HANDLE image_handle, EFI_GRAPHICS_OUTPUT_PROTOCOL** gop);
EFI_STATUS ReadFile(EFI_FILE_PROTOCOL* file, VOID** buffer);
EFI_STATUS OpenBlockIoProtocolForLoadedImage(EFI_HANDLE image_handle, EFI_BLOCK_IO_PROTOCOL** block_io);
EFI_STATUS ReadBlocks(EFI_BLOCK_IO_PROTOCOL* block_io, UINT32 media_id, UINTN read_bytes, VOID** buffer);
void CalcLoadAddressRange(Elf64_Ehdr* ehdr, UINT64* first, UINT64 *last);
void CopyLoadSegment(Elf64_Ehdr* ehdr);
const CHAR16* GetMemoryTypeUnicode(EFI_MEMORY_TYPE type);
const CHAR16* GetPixelFormatUnicode(EFI_GRAPHICS_PIXEL_FORMAT fmt);
void Halt(void);


EFI_STATUS EFIAPI UefiMain(
  EFI_HANDLE image_handle,
  EFI_SYSTEM_TABLE *system_table
) {
  EFI_STATUS status;

  Print(L"Hello, Mikan World!\n");

  CHAR8 memmap_buf[4096*4];
  struct MemoryMap memmap = { sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0 };  
  GetMemoryMap(&memmap);

  EFI_FILE_PROTOCOL* root_dir;
  OpenRootDir(image_handle, &root_dir);

  // GOPを用いたピクセル単位の描画
  EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
  OpenGOP(image_handle, &gop);  
  UINT8* frame_buffer = (UINT8*)gop->Mode->FrameBufferBase;
  for(UINTN i = 0; i < gop->Mode->FrameBufferSize; i++) {
    frame_buffer[i] = 255;
  }

  // GOPの情報を表示
  Print(
    L"Resolution: %ux%u, Pixel Format: %s, %u pixels/line\n", 
    gop->Mode->Info->HorizontalResolution,
    gop->Mode->Info->VerticalResolution,
    GetPixelFormatUnicode(gop->Mode->Info->PixelFormat),
    gop->Mode->Info->PixelsPerScanLine
  );
  Print(
    L"Frame Buffer: 0x%0lx - 0x%0lx, Size %lu bytes\n",
    gop->Mode->FrameBufferBase,
    gop->Mode->FrameBufferBase + gop->Mode->FrameBufferSize,
    gop->Mode->FrameBufferSize
  ); 

  EFI_FILE_PROTOCOL* kernel_file;
  status = root_dir->Open(
    root_dir,
    &kernel_file,
    L"\\kernel.elf",
    EFI_FILE_MODE_READ,
    0
  );
  if(EFI_ERROR(status)) {
    Print(L"failed to open kernel file: %r\n", status);
    Halt();
  }

  VOID* kernel_buffer;
  status = ReadFile(kernel_file, &kernel_buffer);
  if(EFI_ERROR(status)) {
    Print(L"failed to read kernel file: %r\n", status);
    Halt();
  }

  // カーネルを配置するための領域の計算と確保
  Elf64_Ehdr* kernel_ehdr = (Elf64_Ehdr*)kernel_buffer;
  UINT64 kernel_first_addr, kernel_last_addr;
  CalcLoadAddressRange(kernel_ehdr, &kernel_first_addr, &kernel_last_addr);
  UINTN num_pages = (kernel_last_addr - kernel_first_addr + 0xfff) / 0x1000;
  EFI_PHYSICAL_ADDRESS kernel_base_addr = 0x100000;
  status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, num_pages, &kernel_base_addr);
  if(EFI_ERROR(status)) {
    Print(L"failed to allocate kernel area: %r\n", status);
    Halt();
  }

  // カーネルをメモリにロードする
  CopyLoadSegment(kernel_ehdr);

  // エントリポイントの設定
  typedef void ENTRY_POINT(const struct FrameBufferConfig*, const struct MemoryMap*, const VOID*, VOID*);
  ENTRY_POINT* entry_point = (ENTRY_POINT*)kernel_ehdr->e_entry;

  Print(L"Kernel: 0x%0lx\n", kernel_base_addr);
  Print(L"EntryPoint: 0x%0lx\n", kernel_ehdr->e_entry);

  // 名前がfat_diskのファイルを探し、それをブロックデバイスとして
  // デバイスの中身をメモリにロードする。
  // fat_diskが存在しない場合はブートローダのイメージが格納されている
  // デバイスの先頭16MiBをメモリにロードする。
  // ロードされたデータはvolume_imageが指す
  VOID* volume_image;
  EFI_FILE_PROTOCOL* volume_file;
  status = root_dir->Open(
    root_dir, &volume_file, L"\\fat_disk", EFI_FILE_MODE_READ, 0
  );

  if(status == EFI_SUCCESS) {
    status = ReadFile(volume_file, &volume_image);
    if(EFI_ERROR(status)) {
      Print(L"failed to read volume file: %r", status);
      Halt();
    }
  } else {
    Print(L"fat_disk is not found, load bootloader disk\n");

    EFI_BLOCK_IO_PROTOCOL* block_io;
    status = OpenBlockIoProtocolForLoadedImage(image_handle, &block_io);
    if(EFI_ERROR(status)) {
      Print(L"failed to open Block I/O Protocol: %r\n", status);
      Halt();
    }

    Print(L"Block IO Info is loaded\n");

    EFI_BLOCK_IO_MEDIA* media = block_io->Media;
    UINTN volume_bytes = (UINTN)media->BlockSize * (media->LastBlock + 1);
    if(volume_bytes > 16 * 1024 * 1024) {
      volume_bytes = 16 * 1024 * 1024;
    }

    Print(L"Reading %lu bytes (Present %d, BlockSize %u, LastBlock %u)\n", volume_bytes, media->MediaPresent, media->BlockSize, media->LastBlock);
    
    status = ReadBlocks(block_io, media->MediaId, volume_bytes, &volume_image);
    if(EFI_ERROR(status)) {
      Print(L"failed to read blocks: %r\n", status);
      Halt();
    }

    Print(L"Reading Blocks is done\n");
  }

  struct FrameBufferConfig config = {
    (UINT8*)gop->Mode->FrameBufferBase,
    gop->Mode->Info->PixelsPerScanLine,
    gop->Mode->Info->HorizontalResolution,
    gop->Mode->Info->VerticalResolution,
    0
  };

  switch (gop->Mode->Info->PixelFormat) {
    case PixelRedGreenBlueReserved8BitPerColor:
      config.pixel_format = kPixelRGBResv8BitPerColor;
      break;
    case PixelBlueGreenRedReserved8BitPerColor:
      config.pixel_format = kPixelBGRResv8BitPerColor;
      break;
    default:
      Print(L"Unimplemented pixel format: %d\n", gop->Mode->Info->PixelFormat);
      Halt();
  }
  
  status = gBS->ExitBootServices(image_handle, memmap.map_key);
  if(EFI_ERROR(status)) {
    status = GetMemoryMap(&memmap);
    if(EFI_ERROR(status)) {
      Print(L"Failed to get memory map: %r\n", status);
      while(1);
    }

    status = gBS->ExitBootServices(image_handle, memmap.map_key);
    if(EFI_ERROR(status)) {
      Print(L"Could not exit boot service: %r\n", status);
      while(1);
    }
  }

  VOID* acpi_table = NULL;
  for(UINTN i = 0; i < system_table->NumberOfTableEntries; i++) {
    if(CompareGuid(
        &gEfiAcpiTableGuid,
        &system_table->ConfigurationTable[i].VendorGuid
    )) {
      acpi_table = system_table->ConfigurationTable[i].VendorTable;
      break;
    }
  }

  entry_point(&config, &memmap, acpi_table, volume_image);

  Print(L"Exit from kernel (This is fatal)\n");

  while(1);

  return EFI_SUCCESS;
}

EFI_STATUS GetMemoryMap(struct MemoryMap* map) {
  if(map->buffer == NULL) {
    return EFI_BUFFER_TOO_SMALL;
  }

  map->map_size = map->buffer_size;
  return gBS->GetMemoryMap(
    &map->map_size,
    (EFI_MEMORY_DESCRIPTOR*)map->buffer,
    &map->map_key,
    &map->descriptor_size,
    &map->descriptor_version
  );
}

EFI_STATUS SaveMemoryMap(struct MemoryMap* map, EFI_FILE_PROTOCOL* file) {
  CHAR8 buf[256];
  UINTN len;

  CHAR8* header = "Index, Type, Type(name), PhysicalStart, NumberOfPages, Attribute\n";
  len = AsciiStrLen(header);
  file->Write(file, &len, header);

  Print(L"map->buffer = %08lx, map->map_size = %08lx\n", map->buffer, map->map_size);

  EFI_PHYSICAL_ADDRESS iter;
  int i;
  for(
    iter = (EFI_PHYSICAL_ADDRESS)map->buffer, i = 0; 
    iter < (EFI_PHYSICAL_ADDRESS)map->buffer + map->map_size;
    iter += map->descriptor_size, i++
  ) {
    EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)iter;
    len = AsciiSPrint(
      buf, 
      sizeof(buf),
      "%u, %x, %-ls, %08lx, %lx, %lx\n",
      i, desc->Type, GetMemoryTypeUnicode(desc->Type), desc->PhysicalStart, desc->NumberOfPages, desc->Attribute & 0xffffflu
    );
    file->Write(file, &len, buf);
  }

  return EFI_SUCCESS;  
}

EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL** root) {
  EFI_LOADED_IMAGE_PROTOCOL* loaded_image;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;

  gBS->OpenProtocol(
    image_handle,
    &gEfiLoadedImageProtocolGuid,
    (VOID**)&loaded_image,
    image_handle,
    NULL,
    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
  );

  gBS->OpenProtocol(
    loaded_image->DeviceHandle,
    &gEfiSimpleFileSystemProtocolGuid,
    (VOID**)&fs,
    image_handle,
    NULL,
    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
  );

  fs->OpenVolume(fs, root);

  return EFI_SUCCESS;
}

EFI_STATUS OpenGOP(EFI_HANDLE image_handle, EFI_GRAPHICS_OUTPUT_PROTOCOL** gop) {
  UINTN num_gop_handles = 0;
  EFI_HANDLE* gop_handles = NULL;
  gBS->LocateHandleBuffer(
    ByProtocol,
    &gEfiGraphicsOutputProtocolGuid,
    NULL,
    &num_gop_handles,
    &gop_handles
  );

  gBS->OpenProtocol(
    gop_handles[0],
    &gEfiGraphicsOutputProtocolGuid,
    (VOID**)gop,
    image_handle,
    NULL,
    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
  );

  gBS->FreePool(gop_handles);

  return EFI_SUCCESS;
}

EFI_STATUS ReadFile(EFI_FILE_PROTOCOL* file, VOID** buffer) {
  EFI_STATUS status;
  // カーネルのイメージファイルロードとほぼ同じ内容のなので、
  // こちらに説明を移動する。

  // ファイル情報を取得するためのバッファのサイズを指定する。
  UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12;

  // alignasとalignofによってfile_info_bufferが
  // EFI_FILE_INFO構造体のアラインメントと合わない際に発生する問題を
  // 回避することができる。
  alignas(alignof(EFI_FILE_INFO)) UINT8 file_info_buffer[file_info_size];
  status = file->GetInfo(
    file, &gEfiFileInfoGuid,
    &file_info_size, file_info_buffer
  );
  if(EFI_ERROR(status)) {
    return status;
  }

  EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)file_info_buffer;
  UINTN file_size = file_info->FileSize;

  // file->GetInfoによってfile_info_bufferに格納されたファイル情報を用いて
  // （実際にはfile_info_bufferをキャストして入れたfile_infoを用いて）
  // 得た、ファイルサイズ情報からバッファを確保する。 
  Print(L"Allocate memory size: %d\n", file_size); 
  status = gBS->AllocatePool(EfiLoaderData, file_size, buffer);
  if(EFI_ERROR(status)) {
    return status;
  }

  // バッファ上にファイル内容を展開する。
  return file->Read(file, &file_size, *buffer);
}

EFI_STATUS OpenBlockIoProtocolForLoadedImage(
  EFI_HANDLE image_handle, EFI_BLOCK_IO_PROTOCOL** block_io
) {
  EFI_STATUS status;
  EFI_LOADED_IMAGE_PROTOCOL* loaded_image;

  status = gBS->OpenProtocol(
    image_handle,
    &gEfiLoadedImageProtocolGuid,
    (VOID**)&loaded_image,
    image_handle,
    NULL,
    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
  );
  if(EFI_ERROR(status)) {
    return status;
  }

  status = gBS->OpenProtocol(
    loaded_image->DeviceHandle,
    &gEfiBlockIoProtocolGuid,
    (VOID**)block_io,
    image_handle,
    NULL,
    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
  );
  
  return status;
}

EFI_STATUS ReadBlocks(
  EFI_BLOCK_IO_PROTOCOL* block_io,
  UINT32 media_id,
  UINTN read_bytes,
  VOID** buffer
) {
  EFI_STATUS status;

  status = gBS->AllocatePool(EfiLoaderData, read_bytes, buffer);
  if(EFI_ERROR(status)) {
    return status;
  }

  Print(L"execute loading blocks\n");

  status = block_io->ReadBlocks(
    block_io,
    media_id,
    0,
    read_bytes,
    *buffer
  );

  return status;
}

void CalcLoadAddressRange(Elf64_Ehdr* ehdr, UINT64* first, UINT64* last) {
  Elf64_Phdr* phdr = (Elf64_Phdr*)((UINT64)ehdr + ehdr->e_phoff);
  *first = MAX_UINT64;
  *last = 0;

  for(Elf64_Half i= 0; i < ehdr->e_phnum; i++) {
    if(phdr[i].p_type != PT_LOAD) {
      continue;
    }

    *first = MIN(*first, phdr[i].p_vaddr);
    *last = MAX(*last, phdr[i].p_vaddr + phdr[i].p_memsz);
  }
}

void CopyLoadSegment(Elf64_Ehdr* ehdr) {
  Elf64_Phdr* phdr = (Elf64_Phdr*)((UINT64)ehdr + ehdr->e_phoff);
  
  for(Elf64_Half i = 0; i < ehdr->e_phnum; i++) {
    if(phdr[i].p_type != PT_LOAD) {
      continue;
    }

    UINT64 segment_in_file = (UINT64)ehdr + phdr[i].p_offset;    
    CopyMem((VOID*)phdr[i].p_vaddr, (VOID*)segment_in_file, phdr[i].p_filesz);

    UINTN remain_bytes = phdr[i].p_memsz - phdr[i].p_filesz;
    UINTN remain_offset = phdr[i].p_vaddr + phdr[i].p_filesz;
    SetMem((VOID*)remain_offset, remain_bytes, 0);
  }
}

const CHAR16* GetMemoryTypeUnicode(EFI_MEMORY_TYPE type) {
  switch(type) {
    case EfiReservedMemoryType: return L"EfiReservedMemoryType";
    case EfiLoaderCode: return L"EfiLoaderCode";
    case EfiLoaderData: return L"EfiLoaderData";
    case EfiBootServicesCode: return L"EfiBootServicesCode";
    case EfiBootServicesData: return L"EfiBootServicesData";
    case EfiRuntimeServicesCode: return L"EfiRuntimeServicesCode";
    case EfiRuntimeServicesData: return L"EfiRuntimeServicesData";
    case EfiConventionalMemory: return L"EfiConventionalMemory";
    case EfiUnusableMemory: return L"EfiUnusableMemory";
    case EfiACPIReclaimMemory: return L"EfiACPIReclaimMemory";
    case EfiACPIMemoryNVS: return L"EfiACPIMemoryNVS";
    case EfiMemoryMappedIO: return L"EfiMemoryMappedIO";
    case EfiMemoryMappedIOPortSpace: return L"EfiMemoryMappedIOPortSpace";
    case EfiPalCode: return L"EfiPalCode";
    case EfiPersistentMemory: return L"EfiPersistentMemory";
    case EfiMaxMemoryType: return L"EfiMaxMemoryType";
    default: return L"InvalidMemoryType";
  }
}

const CHAR16* GetPixelFormatUnicode(EFI_GRAPHICS_PIXEL_FORMAT fmt) {
  switch(fmt) {
    case PixelRedGreenBlueReserved8BitPerColor:
      return L"PixelRedGreenBlueReserved8BitPerColor";
    case PixelBlueGreenRedReserved8BitPerColor:
      return L"PixelBlueGreenRedReserved8BitPerColor";
    case PixelBitMask:
      return L"PixelBitMask";
    case PixelBltOnly:
      return L"PixelBltOnly";
    case PixelFormatMax:
      return L"PixelFormatMax";
    default:
      return L"InvalidPixelFormat";
  }
}

inline void Halt(void) {
  while(1) {
    __asm__("hlt");
  }
}