#include "ppu_self.h"

static uint16_t read_u16_be(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read_u32_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t read_u64_be(const uint8_t *p)
{
    return ((uint64_t)read_u32_be(p) << 32) | read_u32_be(p + 4);
}

PpuSelfResult ppu_self_parse_header(const uint8_t *bytes, size_t size, PpuSelfView *view)
{
    if (!bytes || !view)
        return PPU_SELF_BAD_ARGUMENT;
    if (size < 32)
        return PPU_SELF_TOO_SMALL;
    if (read_u32_be(bytes) != 0x53434500u)
        return PPU_SELF_BAD_MAGIC;

    PpuSelfView parsed;
    parsed.version = read_u32_be(bytes + 4);
    parsed.key_type = read_u16_be(bytes + 8);
    parsed.category = read_u16_be(bytes + 10);
    parsed.metadata_offset = read_u32_be(bytes + 12);
    parsed.header_size = read_u64_be(bytes + 16);
    parsed.data_size = read_u64_be(bytes + 24);
    if (parsed.metadata_offset < 32 || parsed.metadata_offset >= size)
        return PPU_SELF_BAD_METADATA_OFFSET;
    *view = parsed;
    return PPU_SELF_OK;
}
