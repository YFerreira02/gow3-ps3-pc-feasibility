#ifndef GOW3_PPU_SELF_H
#define GOW3_PPU_SELF_H

#include <stddef.h>
#include <stdint.h>

typedef enum PpuSelfResult {
    PPU_SELF_OK = 0,
    PPU_SELF_BAD_ARGUMENT,
    PPU_SELF_TOO_SMALL,
    PPU_SELF_BAD_MAGIC,
    PPU_SELF_BAD_METADATA_OFFSET,
    PPU_SELF_BAD_EXTENDED_HEADER,
    PPU_SELF_BAD_ELF_METADATA,
    PPU_SELF_BAD_SEGMENT_METADATA
} PpuSelfResult;

typedef struct PpuSelfView {
    uint32_t version;
    uint16_t key_type;
    uint16_t category;
    uint32_t metadata_offset;
    uint64_t header_size;
    uint64_t data_size;
} PpuSelfView;

#define PPU_SELF_MAX_SEGMENTS 64u

typedef struct PpuSelfSegmentInfo {
    uint32_t program_type;
    uint32_t program_flags;
    uint64_t virtual_address;
    uint64_t memory_size;
    uint64_t offset;
    uint64_t size;
    uint32_t compression;
    uint64_t encryption;
} PpuSelfSegmentInfo;

typedef struct PpuSelfImageInfo {
    PpuSelfView fixed;
    uint64_t extended_header_version;
    uint64_t elf_entry;
    uint16_t elf_machine;
    uint16_t program_header_count;
    PpuSelfSegmentInfo segments[PPU_SELF_MAX_SEGMENTS];
} PpuSelfImageInfo;

/* Parses only the fixed, big-endian 32-byte SELF header. Does not decrypt. */
PpuSelfResult ppu_self_parse_header(const uint8_t *bytes, size_t size, PpuSelfView *view);

/* Reads bounded SELF/ELF metadata tables only; segment payloads remain untouched. */
PpuSelfResult ppu_self_inspect_image(const uint8_t *bytes, size_t size,
                                     PpuSelfImageInfo *info);

#endif
