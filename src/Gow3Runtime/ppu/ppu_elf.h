#ifndef GOW3_PPU_ELF_H
#define GOW3_PPU_ELF_H

#include <stddef.h>
#include <stdint.h>

#define PPU_ELF_MAX_LOAD_SEGMENTS 32

typedef enum PpuElfResult {
    PPU_ELF_OK = 0,
    PPU_ELF_BAD_ARGUMENT,
    PPU_ELF_BAD_FORMAT,
    PPU_ELF_UNSUPPORTED_ARCH,
    PPU_ELF_BOUNDS_ERROR,
    PPU_ELF_NO_EXECUTABLE_ENTRY,
    PPU_ELF_TOO_MANY_SEGMENTS
} PpuElfResult;

typedef struct PpuElfSegment {
    uint64_t file_offset;
    uint64_t virtual_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint32_t flags;
} PpuElfSegment;

typedef struct PpuElfView {
    uint64_t entry;
    uint16_t segment_count;
    PpuElfSegment segments[PPU_ELF_MAX_LOAD_SEGMENTS];
} PpuElfView;

PpuElfResult ppu_elf_parse(const uint8_t *file, size_t file_size, PpuElfView *view);
PpuElfResult ppu_elf_load_segments(const uint8_t *file, size_t file_size,
                                  const PpuElfView *view, uint64_t guest_base,
                                  uint8_t *memory, size_t memory_size);

#endif
