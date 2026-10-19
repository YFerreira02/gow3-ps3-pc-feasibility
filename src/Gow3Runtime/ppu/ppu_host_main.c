#include "ppu_core.h"
#include "ppu_elf.h"

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
