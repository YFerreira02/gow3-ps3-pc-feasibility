#include "ppu_core.h"
#include "ppu_elf.h"

typedef void *HANDLE;
__declspec(dllimport) HANDLE __stdcall GetStdHandle(unsigned long);
__declspec(dllimport) int __stdcall WriteFile(HANDLE, const void *, unsigned long, unsigned long *, void *);
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);

static void put_u16_be(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8); p[1] = (uint8_t)value;
}
static void put_u32_be(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24); p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8); p[3] = (uint8_t)value;
}
static void put_u64_be(uint8_t *p, uint64_t value)
{
    put_u32_be(p, (uint32_t)(value >> 32));
    put_u32_be(p + 4, (uint32_t)value);
}

#define D_FORM(op, rt, ra, imm) (((uint32_t)(op) << 26) | ((uint32_t)(rt) << 21) | ((uint32_t)(ra) << 16) | (uint16_t)(imm))
#define EMIT_BE(dst, i, word) do { \
    (dst)[(i) * 4] = (uint8_t)((word) >> 24); \
    (dst)[(i) * 4 + 1] = (uint8_t)((word) >> 16); \
    (dst)[(i) * 4 + 2] = (uint8_t)((word) >> 8); \
    (dst)[(i) * 4 + 3] = (uint8_t)(word); \
} while (0)

static void report(const char *message)
{
    unsigned long written = 0;
    unsigned long length = 0;
    while (message[length]) ++length;
    WriteFile(GetStdHandle((unsigned long)-11), message, length, &written, 0);
}

static int check(int condition, const char *message)
{
    report(condition ? "PASS: " : "FAIL: ");
    report(message);
    report("\r\n");
    return condition;
}

void entry(void)
{
    uint8_t code[16];
    EMIT_BE(code, 0, D_FORM(14, 3, 0, 5));
    EMIT_BE(code, 1, D_FORM(15, 4, 3, 1));
    EMIT_BE(code, 2, D_FORM(24, 4, 5, 0x20));
    EMIT_BE(code, 3, D_FORM(14, 6, 0, -1));

    PpuState state;
    ppu_state_init(&state, code, sizeof(code), 0);
    int ok = 1;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[3] == 5 && state.pc == 4,
                "addi, RA=0, and PC advance");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[4] == 0x10005,
                "addis signed immediate shifted by 16");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0x10025,
                "ori source and destination register fields");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == UINT64_MAX,
                "addi sign extension");
    ok &= check(ppu_step(&state) == PPU_STEP_BAD_ARGUMENT,
                "out-of-range instruction fetch rejected");

    const uint8_t unsupported[] = { 0, 0, 0, 0 };
    ppu_state_init(&state, unsupported, sizeof(unsupported), 0);
    ok &= check(ppu_step(&state) == PPU_STEP_UNSUPPORTED && state.pc == 0,
                "unsupported instruction stops without advancing");

    uint8_t branches[12];
    EMIT_BE(branches, 0, (18u << 26) | (2u << 2));
    EMIT_BE(branches, 1, D_FORM(14, 3, 0, 1));
    EMIT_BE(branches, 2, D_FORM(14, 3, 0, 7));
    ppu_state_init(&state, branches, sizeof(branches), 0);
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 8,
                "relative unconditional branch target");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[3] == 7,
                "branch skips the sequential instruction");

    EMIT_BE(branches, 0, (18u << 26) | (2u << 2) | 1u);
    ppu_state_init(&state, branches, sizeof(branches), 0);
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 8 && state.lr == 4,
                "branch-and-link records return address in LR");

    EMIT_BE(branches, 2, (18u << 26) | (0x00fffffeu << 2));
    ppu_state_init(&state, branches, sizeof(branches), 8);
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 0,
                "negative relative branch displacement");

    EMIT_BE(branches, 2, (18u << 26) | 2u);
    ppu_state_init(&state, branches, sizeof(branches), 8);
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 0,
                "absolute branch target");

    uint8_t conditional[12];
    EMIT_BE(conditional, 0, (16u << 26) | (12u << 21) | (2u << 16) | (2u << 2));
    ppu_state_init(&state, conditional, sizeof(conditional), 0);
    state.cr = 1u << (31u - 2u);
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 8,
                "bc tests selected CR bit and takes branch");
    ppu_state_init(&state, conditional, sizeof(conditional), 0);
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 4,
                "bc falls through when selected CR bit does not match");

    EMIT_BE(conditional, 0, (16u << 26) | (16u << 21) | (2u << 2)); /* bdnz +8 */
    ppu_state_init(&state, conditional, sizeof(conditional), 0);
    state.ctr = 1;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 4 && state.ctr == 0,
                "bc decrements CTR before testing bdnz condition");
    state.pc = 0;
    state.ctr = 2;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 8 && state.ctr == 1,
                "bdnz branches while decremented CTR is nonzero");

    uint8_t indirect[4] = { 0, 0, 0, 0 };
    EMIT_BE(indirect, 0, (19u << 26) | (20u << 21) | (16u << 1)); /* blr */
    ppu_state_init(&state, indirect, sizeof(indirect), 0);
    state.lr = 0x108;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 0x108,
                "bclr branches to aligned LR target");
    EMIT_BE(indirect, 0, (19u << 26) | (20u << 21) | (528u << 1) | 1u); /* bctrl */
    ppu_state_init(&state, indirect, sizeof(indirect), 0);
    state.ctr = 0x208;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 0x208 && state.lr == 4,
                "bcctrl branches through CTR and records LR");

    uint8_t compares[8];
    EMIT_BE(compares, 0, (11u << 26) | (0u << 23) | (1u << 21) | (3u << 16) | 0xffffu);
    ppu_state_init(&state, compares, sizeof(compares), 0);
    state.gpr[3] = UINT64_MAX;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ((state.cr >> 29) & 1u) == 1u,
                "cmpi sets CR0 EQ for equal signed 64-bit operands");
    EMIT_BE(compares, 0, (11u << 26) | (2u << 23) | (1u << 21) | (3u << 16) | 0xffffu);
    ppu_state_init(&state, compares, sizeof(compares), 0);
    state.gpr[3] = UINT64_MAX - 1;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ((state.cr >> 23) & 1u) == 1u,
                "cmpi writes signed less-than to selected CR field");
    EMIT_BE(compares, 0, (10u << 26) | (1u << 23) | (1u << 21) | (4u << 16) | 1u);
    ppu_state_init(&state, compares, sizeof(compares), 0);
    state.gpr[4] = 0x100000000ull;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ((state.cr >> 26) & 1u) == 1u,
                "cmpli sets unsigned greater-than in selected CR field");

    uint8_t memory[12] = { 0, 0, 0, 0, 0x12, 0x34, 0x56, 0x78, 0, 0, 0, 0 };
    uint8_t memory_code[24];
    EMIT_BE(memory_code, 0, D_FORM(14, 3, 0, 4));      /* addi r3,0,4 */
    EMIT_BE(memory_code, 1, D_FORM(32, 4, 3, 0));      /* lwz r4,0(r3) */
    EMIT_BE(memory_code, 2, D_FORM(14, 5, 0, 0x55));   /* addi r5,0,0x55 */
    EMIT_BE(memory_code, 3, D_FORM(36, 5, 0, 4));      /* stw r5,4(0) */
    EMIT_BE(memory_code, 4, D_FORM(34, 6, 0, 7));      /* lbz r6,7(0) */
    ppu_state_init(&state, memory_code, sizeof(memory_code), 0);
    ppu_set_memory(&state, memory, sizeof(memory));
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ppu_step(&state) == PPU_STEP_OK &&
                state.gpr[4] == 0x12345678,
                "lwz reads a big-endian word from mapped memory");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ppu_step(&state) == PPU_STEP_OK &&
                memory[4] == 0 && memory[5] == 0 && memory[6] == 0 && memory[7] == 0x55,
                "stw writes a big-endian word to mapped memory");
    const PpuStepResult lbz_result = ppu_step(&state);
    ok &= check(lbz_result == PPU_STEP_OK, "lbz instruction is supported");
    ok &= check(state.gpr[6] == 0x55, "lbz reads an unsigned byte");

    EMIT_BE(memory_code, 0, D_FORM(38, 7, 0, 2));      /* stb r7,2(0) */
    ppu_state_init(&state, memory_code, 4, 0);
    ppu_set_memory(&state, memory, sizeof(memory));
    state.gpr[7] = 0x1ab;
    ok &= check(ppu_step(&state) == PPU_STEP_OK, "stb instruction is supported");
    ok &= check(memory[2] == 0xab, "stb stores the low byte");

    EMIT_BE(memory_code, 0, D_FORM(32, 4, 3, 0));
    ppu_state_init(&state, memory_code, 4, 0);
    ppu_set_memory(&state, memory, sizeof(memory));
    state.gpr[3] = sizeof(memory) - 2;
    ok &= check(ppu_step(&state) == PPU_STEP_MEMORY_FAULT && state.pc == 0,
                "out-of-range word load faults without advancing PC");
    ppu_state_init(&state, memory_code, 4, 0);
    ppu_set_memory(&state, memory, sizeof(memory));
    state.gpr[3] = 1;
    ok &= check(ppu_step(&state) == PPU_STEP_MEMORY_FAULT && state.pc == 0,
                "misaligned word load faults without advancing PC");

    uint8_t elf[128] = { 0 };
    elf[0] = 0x7f; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
    elf[4] = 2; elf[5] = 2; elf[6] = 1;
    put_u16_be(elf + 16, 2); put_u16_be(elf + 18, 21);
    put_u32_be(elf + 20, 1); put_u64_be(elf + 24, 0x10000);
    put_u64_be(elf + 32, 64); put_u16_be(elf + 52, 64);
    put_u16_be(elf + 54, 56); put_u16_be(elf + 56, 1);
    put_u32_be(elf + 64, 1); put_u32_be(elf + 68, 5);
    put_u64_be(elf + 72, 120); put_u64_be(elf + 80, 0x10000);
    put_u64_be(elf + 96, 8); put_u64_be(elf + 104, 12);
    PpuElfView view;
    ok &= check(ppu_elf_parse(elf, sizeof(elf), &view) == PPU_ELF_OK &&
                view.entry == 0x10000 && view.segment_count == 1 &&
                view.segments[0].file_size == 8 && view.segments[0].memory_size == 12,
                "ELF64 big-endian PPC64 load-segment metadata parsed");
    elf[5] = 1;
    ok &= check(ppu_elf_parse(elf, sizeof(elf), &view) == PPU_ELF_BAD_FORMAT,
                "ELF with unsupported little-endian encoding rejected");
    elf[5] = 2;
    put_u16_be(elf + 18, 3);
    ok &= check(ppu_elf_parse(elf, sizeof(elf), &view) == PPU_ELF_UNSUPPORTED_ARCH,
                "ELF for a non-PPC64 machine rejected");
    put_u16_be(elf + 18, 21);
    put_u64_be(elf + 72, 0xfffffff0u);
    ok &= check(ppu_elf_parse(elf, sizeof(elf), &view) == PPU_ELF_BOUNDS_ERROR,
                "ELF segment outside the file rejected");
    put_u64_be(elf + 72, 120);
    ok &= check(ppu_elf_parse(elf, 90, &view) == PPU_ELF_BOUNDS_ERROR,
                "truncated ELF program-header table rejected");
    put_u64_be(elf + 24, 0x20000);
    ok &= check(ppu_elf_parse(elf, sizeof(elf), &view) == PPU_ELF_NO_EXECUTABLE_ENTRY,
                "ELF entry outside executable load segments rejected");
    put_u64_be(elf + 24, 0x10000);
    elf[120] = 0xde; elf[121] = 0xad; elf[122] = 0xbe; elf[123] = 0xef;
    elf[124] = 0xca; elf[125] = 0xfe; elf[126] = 0xba; elf[127] = 0xbe;
    ok &= check(ppu_elf_parse(elf, sizeof(elf), &view) == PPU_ELF_OK,
                "ELF fixture reparsed before segment loading");
    uint8_t guest_memory[12];
    for (size_t i = 0; i < sizeof(guest_memory); ++i) guest_memory[i] = 0xcc;
    ok &= check(ppu_elf_load_segments(elf, sizeof(elf), &view, 0x10000,
                                      guest_memory, sizeof(guest_memory)) == PPU_ELF_OK &&
                guest_memory[0] == 0xde && guest_memory[3] == 0xef &&
                guest_memory[7] == 0xbe && guest_memory[8] == 0 && guest_memory[11] == 0,
                "ELF segment bytes copied and BSS tail zero-filled");
    ok &= check(ppu_elf_load_segments(elf, sizeof(elf), &view, 0x10000,
                                      guest_memory, sizeof(guest_memory) - 1) == PPU_ELF_BOUNDS_ERROR,
                "ELF segment outside guest memory rejected");
    put_u32_be(elf + 120, D_FORM(14, 3, 0, 5));
    put_u32_be(elf + 124, D_FORM(14, 4, 3, 7));
    ok &= check(ppu_elf_load_segments(elf, sizeof(elf), &view, 0x10000,
                                      guest_memory, sizeof(guest_memory)) == PPU_ELF_OK,
                "synthetic executable segment loaded for PPU integration");
    ppu_state_init(&state, guest_memory, sizeof(guest_memory), view.entry);
    ppu_set_code_base(&state, 0x10000);
    ppu_set_memory_at(&state, guest_memory, sizeof(guest_memory), 0x10000);
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 0x10004 && state.gpr[3] == 5,
                "PPU fetches first instruction from ELF virtual entry address");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 0x10008 && state.gpr[4] == 12,
                "PPU executes sequential instruction from mapped ELF segment");
    ExitProcess(ok ? 0u : 1u);
}
