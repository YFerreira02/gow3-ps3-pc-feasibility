#ifndef GOW3_PPU_SELF_H
#define GOW3_PPU_SELF_H

#include <stddef.h>
#include <stdint.h>

typedef enum PpuSelfResult {
    PPU_SELF_OK = 0,
    PPU_SELF_BAD_ARGUMENT,
    PPU_SELF_TOO_SMALL,
    PPU_SELF_BAD_MAGIC,
    PPU_SELF_BAD_METADATA_OFFSET
} PpuSelfResult;

typedef struct PpuSelfView {
    uint32_t version;
    uint16_t key_type;
    uint16_t category;
    uint32_t metadata_offset;
    uint64_t header_size;
    uint64_t data_size;
} PpuSelfView;

/* Parses only the fixed, big-endian 32-byte SELF header. Does not decrypt. */
PpuSelfResult ppu_self_parse_header(const uint8_t *bytes, size_t size, PpuSelfView *view);

#endif
