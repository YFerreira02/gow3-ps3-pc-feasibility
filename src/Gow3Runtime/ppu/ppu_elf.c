#include "ppu_elf.h"

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

static int range_fits(uint64_t offset, uint64_t length, size_t limit)
{
    return offset <= limit && length <= (uint64_t)limit - offset;
}

PpuElfResult ppu_elf_parse(const uint8_t *file, size_t file_size, PpuElfView *view)
{
    if (!file || !view)
        return PPU_ELF_BAD_ARGUMENT;
    view->entry = 0;
    view->segment_count = 0;
    if (file_size < 64 || file[0] != 0x7f || file[1] != 'E' ||
        file[2] != 'L' || file[3] != 'F' || file[4] != 2 ||
        file[5] != 2 || file[6] != 1 || read_u16_be(file + 16) != 2 ||
        read_u32_be(file + 20) != 1 || read_u16_be(file + 52) != 64)
        return PPU_ELF_BAD_FORMAT;
    if (read_u16_be(file + 18) != 21)
        return PPU_ELF_UNSUPPORTED_ARCH;

    const uint64_t entry = read_u64_be(file + 24);
    const uint64_t phoff = read_u64_be(file + 32);
    const uint16_t phentsize = read_u16_be(file + 54);
    const uint16_t phnum = read_u16_be(file + 56);
    if (phentsize < 56 || phnum == 0 || !range_fits(phoff, (uint64_t)phentsize * phnum, file_size))
        return PPU_ELF_BOUNDS_ERROR;

    int entry_is_executable = 0;
    for (uint16_t i = 0; i < phnum; ++i) {
        const uint8_t *ph = file + (size_t)phoff + (size_t)i * phentsize;
        if (read_u32_be(ph) != 1) /* PT_LOAD */
            continue;
        if (view->segment_count == PPU_ELF_MAX_LOAD_SEGMENTS)
            return PPU_ELF_TOO_MANY_SEGMENTS;

        PpuElfSegment segment;
        segment.flags = read_u32_be(ph + 4);
        segment.file_offset = read_u64_be(ph + 8);
        segment.virtual_address = read_u64_be(ph + 16);
        segment.file_size = read_u64_be(ph + 32);
        segment.memory_size = read_u64_be(ph + 40);
        if (segment.file_size > segment.memory_size ||
            !range_fits(segment.file_offset, segment.file_size, file_size) ||
            segment.virtual_address > UINT64_MAX - segment.memory_size)
            return PPU_ELF_BOUNDS_ERROR;

        if ((segment.flags & 1u) != 0 && entry >= segment.virtual_address &&
            entry < segment.virtual_address + segment.memory_size)
            entry_is_executable = 1;
        view->segments[view->segment_count++] = segment;
    }
    if (!entry_is_executable)
        return PPU_ELF_NO_EXECUTABLE_ENTRY;
    view->entry = entry;
    return PPU_ELF_OK;
}

PpuElfResult ppu_elf_load_segments(const uint8_t *file, size_t file_size,
                                  const PpuElfView *view, uint64_t guest_base,
                                  uint8_t *memory, size_t memory_size)
{
    if (!file || !view || !memory || view->segment_count > PPU_ELF_MAX_LOAD_SEGMENTS)
        return PPU_ELF_BAD_ARGUMENT;
    for (uint16_t i = 0; i < view->segment_count; ++i) {
        const PpuElfSegment *segment = &view->segments[i];
        if (segment->virtual_address < guest_base ||
            !range_fits(segment->file_offset, segment->file_size, file_size) ||
            segment->file_size > segment->memory_size ||
            segment->virtual_address > UINT64_MAX - segment->memory_size)
            return PPU_ELF_BOUNDS_ERROR;
        const uint64_t memory_offset = segment->virtual_address - guest_base;
        if (!range_fits(memory_offset, segment->memory_size, memory_size))
            return PPU_ELF_BOUNDS_ERROR;
    }

    for (uint16_t i = 0; i < view->segment_count; ++i) {
        const PpuElfSegment *segment = &view->segments[i];
        const uint64_t memory_offset = segment->virtual_address - guest_base;
        uint8_t *destination = memory + (size_t)memory_offset;
        const uint8_t *source = file + (size_t)segment->file_offset;
        for (uint64_t byte = 0; byte < segment->file_size; ++byte)
            destination[byte] = source[byte];
        for (uint64_t byte = segment->file_size; byte < segment->memory_size; ++byte)
            destination[byte] = 0;
    }
    return PPU_ELF_OK;
}
