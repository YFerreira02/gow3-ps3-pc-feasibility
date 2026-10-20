#include "ppu_core.h"
#include "ppu_elf.h"
#include "ppu_iso.h"
#include "ppu_self.h"

typedef void *HANDLE;
typedef uint16_t WCHAR16;
__declspec(dllimport) WCHAR16 *__stdcall GetCommandLineW(void);
__declspec(dllimport) WCHAR16 **__stdcall CommandLineToArgvW(const WCHAR16 *, int *);
__declspec(dllimport) void *__stdcall LocalFree(void *);
__declspec(dllimport) HANDLE __stdcall CreateFileW(const WCHAR16 *, uint32_t, uint32_t,
    void *, uint32_t, uint32_t, HANDLE);
__declspec(dllimport) int __stdcall GetFileSizeEx(HANDLE, int64_t *);
__declspec(dllimport) int __stdcall ReadFile(HANDLE, void *, uint32_t, uint32_t *, void *);
__declspec(dllimport) int __stdcall CloseHandle(HANDLE);
__declspec(dllimport) HANDLE __stdcall CreateFileMappingW(HANDLE, void *, uint32_t, uint32_t, uint32_t, const WCHAR16 *);
__declspec(dllimport) void *__stdcall MapViewOfFile(HANDLE, uint32_t, uint32_t, uint32_t, size_t);
__declspec(dllimport) int __stdcall UnmapViewOfFile(const void *);
__declspec(dllimport) void *__stdcall VirtualAlloc(void *, size_t, uint32_t, uint32_t);
__declspec(dllimport) int __stdcall VirtualFree(void *, size_t, uint32_t);
__declspec(dllimport) HANDLE __stdcall GetStdHandle(int32_t);
__declspec(dllimport) int __stdcall WriteFile(HANDLE, const void *, uint32_t, uint32_t *, void *);
__declspec(dllimport) void __stdcall ExitProcess(uint32_t);

#define INVALID_HANDLE ((HANDLE)(intptr_t)-1)
#define GENERIC_READ 0x80000000u
#define FILE_SHARE_READ 1u
#define OPEN_EXISTING 3u
#define FILE_ATTRIBUTE_NORMAL 0x80u
#define MEM_COMMIT 0x1000u
#define MEM_RESERVE 0x2000u
#define MEM_RELEASE 0x8000u
#define PAGE_READWRITE 4u
#define PAGE_READONLY 2u
#define FILE_MAP_READ 4u
#define STD_OUTPUT_HANDLE ((int32_t)-11)
#define MAX_ELF_FILE_SIZE (1024ull * 1024ull * 1024ull)
#define MAX_GUEST_SPAN (512ull * 1024ull * 1024ull)
#define DEFAULT_INSTRUCTION_BUDGET 10000000ull

static HANDLE output_handle(void)
{
    return GetStdHandle(STD_OUTPUT_HANDLE);
}

static void write_text(const char *text)
{
    uint32_t ignored = 0;
    uint32_t length = 0;
    while (text[length] != 0)
        ++length;
    if (length != 0)
        WriteFile(output_handle(), text, length, &ignored, 0);
}

static void write_u64(uint64_t value)
{
    char digits[24];
    uint32_t count = 0;
    do {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0 && count < sizeof(digits));
    while (count != 0) {
        const char digit = digits[--count];
        uint32_t ignored = 0;
        WriteFile(output_handle(), &digit, 1, &ignored, 0);
    }
}

static void write_hex_u64(uint64_t value)
{
    static const char alphabet[] = "0123456789abcdef";
    char digits[16];
    uint32_t count = 0;
    do {
        digits[count++] = alphabet[value & 15u];
        value >>= 4;
    } while (value != 0 && count < sizeof(digits));
    while (count != 0) {
        const char digit = digits[--count];
        uint32_t ignored = 0;
        WriteFile(output_handle(), &digit, 1, &ignored, 0);
    }
}

static int parse_u64(const WCHAR16 *text, uint64_t *value)
{
    uint64_t parsed = 0;
    if (!text || !*text)
        return 0;
    for (uint32_t i = 0; text[i] != 0; ++i) {
        if (text[i] < '0' || text[i] > '9')
            return 0;
        const uint32_t digit = text[i] - '0';
        if (parsed > (UINT64_MAX - digit) / 10u)
            return 0;
        parsed = parsed * 10u + digit;
    }
    if (parsed == 0)
        return 0;
    *value = parsed;
    return 1;
}

static int load_file(const WCHAR16 *path, uint8_t **bytes, size_t *length)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE)
        return 0;
    int64_t file_size = 0;
    if (!GetFileSizeEx(file, &file_size) || file_size <= 0 ||
        (uint64_t)file_size > MAX_ELF_FILE_SIZE) {
        CloseHandle(file);
        return 0;
    }
    uint8_t *buffer = (uint8_t *)VirtualAlloc(0, (size_t)file_size,
                                               MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!buffer) {
        CloseHandle(file);
        return 0;
    }
    size_t offset = 0;
    while (offset < (size_t)file_size) {
        uint32_t request = (uint32_t)((size_t)file_size - offset);
        if (request > 1024u * 1024u)
            request = 1024u * 1024u;
        uint32_t received = 0;
        if (!ReadFile(file, buffer + offset, request, &received, 0) || received == 0) {
            VirtualFree(buffer, 0, MEM_RELEASE);
            CloseHandle(file);
            return 0;
        }
        offset += received;
    }
    CloseHandle(file);
    *bytes = buffer;
    *length = (size_t)file_size;
    return 1;
}

static int path_has_iso_extension(const WCHAR16 *path)
{
    uint32_t length = 0;
    while (path[length] != 0) ++length;
    if (length < 4) return 0;
    const WCHAR16 *suffix = path + length - 4;
    return suffix[0] == '.' && (suffix[1] == 'i' || suffix[1] == 'I') &&
           (suffix[2] == 's' || suffix[2] == 'S') && (suffix[3] == 'o' || suffix[3] == 'O');
}

static int probe_iso(const WCHAR16 *path)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE) {
        write_text("Could not open the ISO in read-only mode.\r\n");
        return 3;
    }
    int64_t signed_size = 0;
    if (!GetFileSizeEx(file, &signed_size) || signed_size <= 0) {
        CloseHandle(file);
        write_text("Could not read the ISO file size.\r\n");
        return 3;
    }
    HANDLE mapping = CreateFileMappingW(file, 0, PAGE_READONLY, 0, 0, 0);
    if (!mapping) {
        CloseHandle(file);
        write_text("Could not create a read-only mapping for the ISO.\r\n");
        return 3;
    }
    const uint8_t *image = (const uint8_t *)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (!image) {
        CloseHandle(mapping);
        CloseHandle(file);
        write_text("Could not map the ISO image into memory.\r\n");
        return 3;
    }
    PpuIsoFile eboot;
    const PpuIsoResult result = ppu_iso_find_ps3_eboot(image, (size_t)signed_size, &eboot);
    if (result != PPU_ISO_OK) {
        write_text("ISO/9660 scan did not find PS3_GAME/USRDIR/EBOOT.BIN. Result code: ");
        write_u64((uint64_t)result);
        write_text("\r\n");
    } else {
        const uint8_t *header = image + (size_t)eboot.offset;
        write_text("Found EBOOT.BIN at ISO byte offset "); write_u64(eboot.offset);
        write_text(", size "); write_u64(eboot.size); write_text(" bytes.\r\nHeader: ");
        if (eboot.size >= 4 && header[0] == 0x7f && header[1] == 'E' && header[2] == 'L' && header[3] == 'F')
            write_text("raw ELF\r\n");
        else if (eboot.size >= 4 && header[0] == 'S' && header[1] == 'C' && header[2] == 'E' && header[3] == 0) {
            PpuSelfView self;
            const PpuSelfResult self_result = ppu_self_parse_header(header, (size_t)eboot.size, &self);
            if (self_result == PPU_SELF_OK) {
                write_text("SCE SELF header parsed (read-only): version "); write_u64(self.version);
                write_text(", key type "); write_u64(self.key_type);
                write_text(", category "); write_u64(self.category);
                write_text(", metadata offset "); write_u64(self.metadata_offset);
                write_text(", header size "); write_u64(self.header_size);
                write_text(", data size "); write_u64(self.data_size); write_text(" bytes.\r\n");
            } else {
                write_text("SCE magic found, but fixed SELF header validation failed. Code: ");
                write_u64((uint64_t)self_result); write_text("\r\n");
            }
            static PpuSelfImageInfo details;
            const PpuSelfResult details_result = ppu_self_inspect_image(header, (size_t)eboot.size, &details);
            if (details_result == PPU_SELF_OK) {
                write_text("SELF ELF metadata: machine "); write_u64(details.elf_machine);
                write_text(", entry 0x"); write_hex_u64(details.elf_entry);
                write_text(", program headers/segments "); write_u64(details.program_header_count);
                write_text(". Segment descriptors (payload offset, size, compression, encryption marker):\r\n");
                for (uint16_t i = 0; i < details.program_header_count; ++i) {
                    const PpuSelfSegmentInfo *segment = &details.segments[i];
                    write_text("  "); write_u64(i); write_text(": vaddr 0x"); write_hex_u64(segment->virtual_address);
                    write_text(", mem 0x"); write_hex_u64(segment->memory_size);
                    write_text(", payload 0x"); write_hex_u64(segment->offset);
                    write_text(", 0x"); write_hex_u64(segment->size);
                    write_text(", "); write_u64(segment->compression);
                    write_text(", "); write_u64(segment->encryption); write_text("\r\n");
                }
            } else {
                write_text("SELF ELF metadata table validation failed. Code: ");
                write_u64((uint64_t)details_result); write_text("\r\n");
            }
            write_text("Metadata only: encrypted certification and ELF payloads are not loaded.\r\n");
        }
        else
            write_text("unrecognized\r\n");
        write_text("The disc image was only read through a memory map; no files were extracted or copied.\r\n");
    }
    UnmapViewOfFile(image);
    CloseHandle(mapping);
    CloseHandle(file);
    return result == PPU_ISO_OK ? 0 : 4;
}

static int host_main(void)
{
    int argument_count = 0;
    WCHAR16 **arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
    if (!arguments || argument_count < 2 || argument_count > 3) {
        write_text("Usage: Gow3PpuHost.exe <decrypted-ppc64-elf> [instruction-budget]\r\n");
        if (arguments)
            LocalFree(arguments);
        return 2;
    }

    uint64_t instruction_budget = DEFAULT_INSTRUCTION_BUDGET;
    if (argument_count == 3 && !parse_u64(arguments[2], &instruction_budget)) {
        write_text("Invalid instruction budget. Use a positive decimal integer.\r\n");
        LocalFree(arguments);
        return 2;
    }

    if (path_has_iso_extension(arguments[1])) {
        const int result = probe_iso(arguments[1]);
        LocalFree(arguments);
        return result;
    }

    uint8_t *file_bytes = 0;
    size_t file_size = 0;
    if (!load_file(arguments[1], &file_bytes, &file_size)) {
        write_text("Could not read the ELF file (check path, permissions, and file size).\r\n");
        LocalFree(arguments);
        return 3;
    }
    LocalFree(arguments);

    PpuElfView elf;
    const PpuElfResult parse_result = ppu_elf_parse(file_bytes, file_size, &elf);
    if (parse_result != PPU_ELF_OK) {
        write_text("Input rejected: expected a valid big-endian PPC64 ELF with an executable entry. Code: ");
        write_u64((uint64_t)parse_result);
        write_text("\r\n");
        VirtualFree(file_bytes, 0, MEM_RELEASE);
        return 4;
    }

    uint64_t guest_base = UINT64_MAX;
    uint64_t guest_end = 0;
    for (uint16_t i = 0; i < elf.segment_count; ++i) {
        const PpuElfSegment *segment = &elf.segments[i];
        if (segment->virtual_address < guest_base)
            guest_base = segment->virtual_address;
        const uint64_t segment_end = segment->virtual_address + segment->memory_size;
        if (segment_end > guest_end)
            guest_end = segment_end;
    }
    if (guest_base == UINT64_MAX || guest_end <= guest_base ||
        guest_end - guest_base > MAX_GUEST_SPAN) {
        write_text("ELF load span is empty or exceeds the 512 MiB prototype limit.\r\n");
        VirtualFree(file_bytes, 0, MEM_RELEASE);
        return 5;
    }
    const size_t guest_size = (size_t)(guest_end - guest_base);
    uint8_t *guest_memory = (uint8_t *)VirtualAlloc(0, guest_size,
                                                    MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!guest_memory) {
        write_text("Could not reserve guest memory for ELF segments.\r\n");
        VirtualFree(file_bytes, 0, MEM_RELEASE);
        return 6;
    }
    const PpuElfResult load_result = ppu_elf_load_segments(
        file_bytes, file_size, &elf, guest_base, guest_memory, guest_size);
    VirtualFree(file_bytes, 0, MEM_RELEASE);
    if (load_result != PPU_ELF_OK) {
        write_text("ELF segment mapping failed. Code: ");
        write_u64((uint64_t)load_result);
        write_text("\r\n");
        VirtualFree(guest_memory, 0, MEM_RELEASE);
        return 7;
    }

    PpuState cpu;
    ppu_state_init(&cpu, guest_memory, guest_size, elf.entry);
    ppu_set_code_base(&cpu, guest_base);
    ppu_set_memory_at(&cpu, guest_memory, guest_size, guest_base);
    uint64_t executed = 0;
    const PpuStepResult run_result = ppu_run(&cpu, instruction_budget, &executed);
    write_text("PPU stopped with status ");
    write_u64((uint64_t)run_result);
    write_text(" after ");
    write_u64(executed);
    write_text(" instructions; PC=0x");
    write_hex_u64(cpu.pc);
    write_text(".\r\n");
    write_text("Prototype only: PS3 syscalls, SPU, RSX, and game services are not implemented.\r\n");
    VirtualFree(guest_memory, 0, MEM_RELEASE);
    return run_result == PPU_STEP_LIMIT_REACHED || run_result == PPU_STEP_SYSCALL ? 0 : 8;
}

void entry(void)
{
    ExitProcess((uint32_t)host_main());
}
