#include "ppu_core.h"

typedef void *HANDLE;
__declspec(dllimport) HANDLE __stdcall GetStdHandle(unsigned long);
__declspec(dllimport) int __stdcall WriteFile(HANDLE, const void *, unsigned long, unsigned long *, void *);
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);

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
    ExitProcess(ok ? 0u : 1u);
}
