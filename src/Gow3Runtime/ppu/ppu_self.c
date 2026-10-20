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

static int span_in_file(size_t file_size, uint64_t offset, uint64_t length)
{
    return offset <= (uint64_t)file_size && length <= (uint64_t)file_size - offset;
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

PpuSelfResult ppu_self_inspect_image(const uint8_t *bytes, size_t size,
                                    PpuSelfImageInfo *info)
{
    if (!bytes || !info)
        return PPU_SELF_BAD_ARGUMENT;
    PpuSelfView fixed;
    PpuSelfResult result = ppu_self_parse_header(bytes, size, &fixed);
    if (result != PPU_SELF_OK)
        return result;
    if (!span_in_file(size, 32, 0x50) || fixed.header_size < 32u + 0x50u)
        return PPU_SELF_BAD_EXTENDED_HEADER;

    info->fixed.version = fixed.version;
    info->fixed.key_type = fixed.key_type;
    info->fixed.category = fixed.category;
    info->fixed.metadata_offset = fixed.metadata_offset;
    info->fixed.header_size = fixed.header_size;
    info->fixed.data_size = fixed.data_size;
    info->extended_header_version = 0;
    info->elf_entry = 0;
    info->elf_machine = 0;
    info->program_header_count = 0;

    const uint8_t *extended = bytes + 32;
    info->extended_header_version = read_u64_be(extended);
    const uint64_t elf_header_offset = read_u64_be(extended + 16);
    const uint64_t phdr_offset = read_u64_be(extended + 24);
    const uint64_t segment_ext_offset = read_u64_be(extended + 40);
    if (info->extended_header_version != 3 ||
        !span_in_file(size, elf_header_offset, 64) ||
        !span_in_file(size, segment_ext_offset, 0))
        return PPU_SELF_BAD_EXTENDED_HEADER;
    if (elf_header_offset < 32u + 0x50u || elf_header_offset >= fixed.metadata_offset ||
        segment_ext_offset >= fixed.metadata_offset || fixed.header_size > size ||
        fixed.data_size > (uint64_t)size - fixed.header_size)
        return PPU_SELF_BAD_EXTENDED_HEADER;

    const uint8_t *elf = bytes + (size_t)elf_header_offset;
    if (elf[0] != 0x7f || elf[1] != 'E' || elf[2] != 'L' || elf[3] != 'F' ||
        elf[4] != 2 || elf[5] != 2 || elf[6] != 1 || read_u16_be(elf + 52) != 64)
        return PPU_SELF_BAD_ELF_METADATA;
    info->elf_machine = read_u16_be(elf + 18);
    info->elf_entry = read_u64_be(elf + 24);
    const uint64_t elf_phoff = read_u64_be(elf + 32);
    const uint16_t elf_phentsize = read_u16_be(elf + 54);
    info->program_header_count = read_u16_be(elf + 56);
    if (info->elf_machine != 21 || elf_phoff > UINT64_MAX - elf_header_offset ||
        elf_phoff != 64 || phdr_offset != elf_header_offset + elf_phoff ||
        elf_phentsize != 56 || info->program_header_count == 0 ||
        info->program_header_count > PPU_SELF_MAX_SEGMENTS ||
        !span_in_file(size, phdr_offset, (uint64_t)info->program_header_count * 56u) ||
        !span_in_file(size, segment_ext_offset, (uint64_t)info->program_header_count * 32u) ||
        segment_ext_offset + (uint64_t)info->program_header_count * 32u > fixed.metadata_offset)
        return PPU_SELF_BAD_ELF_METADATA;

    for (uint16_t i = 0; i < info->program_header_count; ++i) {
        const uint8_t *segment = bytes + (size_t)(segment_ext_offset + (uint64_t)i * 32u);
        PpuSelfSegmentInfo *out = &info->segments[i];
        const uint8_t *program_header = bytes + (size_t)(phdr_offset + (uint64_t)i * 56u);
        out->program_type = read_u32_be(program_header);
        out->program_flags = read_u32_be(program_header + 4);
        out->virtual_address = read_u64_be(program_header + 16);
        out->memory_size = read_u64_be(program_header + 40);
        out->offset = read_u64_be(segment);
        out->size = read_u64_be(segment + 8);
        out->compression = read_u32_be(segment + 16);
        out->encryption = read_u64_be(segment + 24);
        if (!span_in_file(size, out->offset, out->size) ||
            (out->size != 0 && out->offset < fixed.header_size) ||
            out->memory_size < read_u64_be(program_header + 32) ||
            out->virtual_address > UINT64_MAX - out->memory_size)
            return PPU_SELF_BAD_SEGMENT_METADATA;
    }
    return PPU_SELF_OK;
}
