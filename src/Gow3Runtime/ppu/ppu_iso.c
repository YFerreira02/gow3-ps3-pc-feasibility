#include "ppu_iso.h"

#define ISO_SECTOR_SIZE 2048u
#define ISO_MAX_DIRECTORY_SIZE (64u * 1024u * 1024u)

typedef struct IsoRecord {
    uint32_t extent;
    uint32_t size;
    uint8_t flags;
    const uint8_t *name;
    uint8_t name_length;
} IsoRecord;

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int range_fits(uint64_t offset, uint64_t length, size_t total)
{
    return offset <= total && length <= (uint64_t)total - offset;
}

static int read_record(const uint8_t *bytes, size_t available, IsoRecord *record)
{
    if (!bytes || !record || available < 34 || bytes[0] < 34 || bytes[0] > available)
        return 0;
    const uint8_t name_length = bytes[32];
    if (name_length == 0 || (uint16_t)33u + name_length > bytes[0])
        return 0;
    record->extent = read_le32(bytes + 2);
    record->size = read_le32(bytes + 10);
    record->flags = bytes[25];
    record->name = bytes + 33;
    record->name_length = name_length;
    return 1;
}

static uint8_t ascii_upper(uint8_t value)
{
    return value >= 'a' && value <= 'z' ? (uint8_t)(value - ('a' - 'A')) : value;
}

static int name_matches(const IsoRecord *record, const char *wanted)
{
    uint32_t wanted_length = 0;
    while (wanted[wanted_length] != 0) ++wanted_length;
    uint32_t actual_length = record->name_length;
    if (actual_length == wanted_length + 2u && record->name[wanted_length] == ';' &&
        record->name[wanted_length + 1u] == '1')
        actual_length -= 2u;
    if (actual_length != wanted_length)
        return 0;
    for (uint32_t i = 0; i < wanted_length; ++i)
        if (ascii_upper(record->name[i]) != ascii_upper((uint8_t)wanted[i]))
            return 0;
    return 1;
}

static PpuIsoResult find_child(const uint8_t *image, size_t image_size,
                               uint32_t parent_extent, uint32_t parent_size,
                               const char *name, int want_directory, IsoRecord *found)
{
    if (parent_size > ISO_MAX_DIRECTORY_SIZE)
        return PPU_ISO_BAD_DIRECTORY;
    const uint64_t directory_offset = (uint64_t)parent_extent * ISO_SECTOR_SIZE;
    if (!range_fits(directory_offset, parent_size, image_size))
        return PPU_ISO_BAD_DIRECTORY;
    uint32_t position = 0;
    while (position < parent_size) {
        const uint8_t *entry = image + (size_t)directory_offset + position;
        const uint8_t entry_length = entry[0];
        if (entry_length == 0) {
            const uint32_t next_sector = ((position / ISO_SECTOR_SIZE) + 1u) * ISO_SECTOR_SIZE;
            if (next_sector <= position) return PPU_ISO_BAD_DIRECTORY;
            position = next_sector;
            continue;
        }
        if (entry_length < 34 || entry_length > parent_size - position)
            return PPU_ISO_BAD_DIRECTORY;
        IsoRecord record;
        if (!read_record(entry, entry_length, &record))
            return PPU_ISO_BAD_DIRECTORY;
        const int is_directory = (record.flags & 2u) != 0;
        if (is_directory == want_directory && name_matches(&record, name)) {
            *found = record;
            return PPU_ISO_OK;
        }
        position += entry_length;
    }
    return PPU_ISO_GAME_PATH_NOT_FOUND;
}

PpuIsoResult ppu_iso_find_ps3_eboot(const uint8_t *image, size_t image_size,
                                   PpuIsoFile *file)
{
    if (!image || !file)
        return PPU_ISO_BAD_ARGUMENT;
    file->offset = 0;
    file->size = 0;
    if (!range_fits(16u * ISO_SECTOR_SIZE, ISO_SECTOR_SIZE, image_size))
        return PPU_ISO_NO_ISO9660_DESCRIPTOR;

    const uint8_t *primary = 0;
    for (uint32_t sector = 16; sector < 64; ++sector) {
        if (!range_fits((uint64_t)sector * ISO_SECTOR_SIZE, ISO_SECTOR_SIZE, image_size))
            break;
        const uint8_t *descriptor = image + (size_t)sector * ISO_SECTOR_SIZE;
        if (descriptor[0] == 255u)
            break;
        if (descriptor[0] == 1u && descriptor[1] == 'C' && descriptor[2] == 'D' &&
            descriptor[3] == '0' && descriptor[4] == '0' && descriptor[5] == '1' &&
            descriptor[6] == 1u) {
            primary = descriptor;
            break;
        }
    }
    if (!primary)
        return PPU_ISO_NO_ISO9660_DESCRIPTOR;

    IsoRecord root, game, usrdir, eboot;
    if (!read_record(primary + 156, ISO_SECTOR_SIZE - 156, &root) ||
        (root.flags & 2u) == 0)
        return PPU_ISO_BAD_DIRECTORY;
    PpuIsoResult result = find_child(image, image_size, root.extent, root.size,
                                     "PS3_GAME", 1, &game);
    if (result != PPU_ISO_OK) return result;
    result = find_child(image, image_size, game.extent, game.size, "USRDIR", 1, &usrdir);
    if (result != PPU_ISO_OK) return result;
    result = find_child(image, image_size, usrdir.extent, usrdir.size, "EBOOT.BIN", 0, &eboot);
    if (result != PPU_ISO_OK) return result;
    const uint64_t offset = (uint64_t)eboot.extent * ISO_SECTOR_SIZE;
    if (!range_fits(offset, eboot.size, image_size))
        return PPU_ISO_BAD_DIRECTORY;
    file->offset = offset;
    file->size = eboot.size;
    return PPU_ISO_OK;
}
