#ifndef GOW3_PPU_ISO_H
#define GOW3_PPU_ISO_H

#include <stddef.h>
#include <stdint.h>

typedef enum PpuIsoResult {
    PPU_ISO_OK = 0,
    PPU_ISO_BAD_ARGUMENT,
    PPU_ISO_NO_ISO9660_DESCRIPTOR,
    PPU_ISO_BAD_DIRECTORY,
    PPU_ISO_GAME_PATH_NOT_FOUND
} PpuIsoResult;

typedef struct PpuIsoFile {
    uint64_t offset;
    uint64_t size;
} PpuIsoFile;

/* Finds PS3_GAME/USRDIR/EBOOT.BIN in a read-only ISO9660 image view. */
PpuIsoResult ppu_iso_find_ps3_eboot(const uint8_t *image, size_t image_size,
                                   PpuIsoFile *file);

#endif
