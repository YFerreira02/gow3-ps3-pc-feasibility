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
#define X_FORM(rt, ra, rb, xo, rc) ((31u << 26) | ((uint32_t)(rt) << 21) | \
    ((uint32_t)(ra) << 16) | ((uint32_t)(rb) << 11) | ((uint32_t)(xo) << 1) | (rc))
#define ISEL_FORM(rt, ra, rb, bc) ((31u << 26) | ((uint32_t)(rt) << 21) | \
    ((uint32_t)(ra) << 16) | ((uint32_t)(rb) << 11) | ((uint32_t)(bc) << 6) | (15u << 1))
#define ROT_FORM(op, rs, ra, sh, mb, me, rc) (((uint32_t)(op) << 26) | \
    ((uint32_t)(rs) << 21) | ((uint32_t)(ra) << 16) | ((uint32_t)(sh) << 11) | \
    ((uint32_t)(mb) << 6) | ((uint32_t)(me) << 1) | (rc))
#define SPR_FORM(rt, spr, xo) ((31u << 26) | ((uint32_t)(rt) << 21) | \
    (((uint32_t)(spr) & 31u) << 16) | (((uint32_t)(spr) >> 5) << 11) | ((uint32_t)(xo) << 1))
#define DS_FORM(op, rt, ra, disp, xo) (((uint32_t)(op) << 26) | ((uint32_t)(rt) << 21) | \
    ((uint32_t)(ra) << 16) | ((uint16_t)(disp) & 0xfffcu) | (uint32_t)(xo))
#define CR_FORM(bt, ba, bb, xo) ((19u << 26) | ((uint32_t)(bt) << 21) | \
    ((uint32_t)(ba) << 16) | ((uint32_t)(bb) << 11) | ((uint32_t)(xo) << 1))
#define FP_A(rt, ra, rb, frc, xo) ((63u << 26) | ((uint32_t)(rt) << 21) | \
    ((uint32_t)(ra) << 16) | ((uint32_t)(rb) << 11) | ((uint32_t)(frc) << 6) | ((uint32_t)(xo) << 1))
#define FP_S(rt, ra, rb, frc, xo) ((59u << 26) | ((uint32_t)(rt) << 21) | \
    ((uint32_t)(ra) << 16) | ((uint32_t)(rb) << 11) | ((uint32_t)(frc) << 6) | ((uint32_t)(xo) << 1))
#define FP_X(rt, rb, xo) ((63u << 26) | ((uint32_t)(rt) << 21) | \
    ((uint32_t)(rb) << 11) | ((uint32_t)(xo) << 1))
#define MD_FORM(rs, ra, sh, mb, xo, rc) ((30u << 26) | ((uint32_t)(rs) << 21) | \
    ((uint32_t)(ra) << 16) | (((uint32_t)(sh) & 31u) << 11) | \
    (((uint32_t)(mb) & 63u) << 5) | (((uint32_t)(xo) & 7u) << 2) | \
    (((uint32_t)(sh) & 32u) >> 4) | (rc))
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
    static uint8_t code[16];
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

    static uint8_t immediate_carry[16];
    EMIT_BE(immediate_carry, 0, D_FORM(12, 5, 3, 1)); /* addic */
    EMIT_BE(immediate_carry, 1, D_FORM(13, 6, 4, 1)); /* addic. */
    EMIT_BE(immediate_carry, 2, D_FORM(8, 7, 8, 5));  /* subfic */
    EMIT_BE(immediate_carry, 3, D_FORM(8, 9, 8, 5));  /* subfic borrow */
    ppu_state_init(&state, immediate_carry, sizeof(immediate_carry), 0);
    state.gpr[3] = 5; state.gpr[4] = UINT64_MAX; state.gpr[8] = 3;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 6 && state.xer_ca == 0,
                "addic adds a signed immediate without carry");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == 0 && state.xer_ca == 1 &&
                ((state.cr >> 29) & 1u) == 1u,
                "addic. updates carry and CR0 on unsigned overflow");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == 2 && state.xer_ca == 1,
                "subfic computes immediate minus RA and sets carry without borrow");
    state.gpr[8] = 6;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[9] == UINT64_MAX && state.xer_ca == 0,
                "subfic clears carry when the subtraction borrows");

    const uint8_t unsupported[] = { 0, 0, 0, 0 };
    ppu_state_init(&state, unsupported, sizeof(unsupported), 0);
    ok &= check(ppu_step(&state) == PPU_STEP_UNSUPPORTED && state.pc == 0,
                "unsupported instruction stops without advancing");

    static uint8_t syscall_code[4];
    EMIT_BE(syscall_code, 0, (17u << 26) | 2u); /* sc */
    ppu_state_init(&state, syscall_code, sizeof(syscall_code), 0);
    state.gpr[11] = 0x123;
    ok &= check(ppu_step(&state) == PPU_STEP_SYSCALL && state.pc == 4 && state.gpr[11] == 0x123,
                "sc advances to the return PC and yields to the PS3 service dispatcher");
    ppu_state_init(&state, syscall_code, sizeof(syscall_code), 0);
    uint64_t executed = 0;
    ok &= check(ppu_run(&state, 1, &executed) == PPU_STEP_SYSCALL && executed == 1,
                "bounded PPU runner stops at the syscall handoff");

    static uint8_t loop_code[8];
    EMIT_BE(loop_code, 0, D_FORM(14, 3, 3, 1));
    EMIT_BE(loop_code, 1, (18u << 26) | (0x00ffffffu << 2)); /* b -4 */
    ppu_state_init(&state, loop_code, sizeof(loop_code), 0);
    ok &= check(ppu_run(&state, 10, &executed) == PPU_STEP_LIMIT_REACHED &&
                executed == 10 && state.gpr[3] == 5 && state.pc == 0,
                "bounded PPU runner yields after exactly the instruction budget");

    static uint8_t branches[12];
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

    static uint8_t conditional[12];
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

    static uint8_t indirect[4] = { 0, 0, 0, 0 };
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

    static uint8_t compares[8];
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

    static uint8_t reg_compares[8];
    EMIT_BE(reg_compares, 0, X_FORM((2u << 2), 3, 4, 0, 0));  /* cmpw cr2,r3,r4 */
    EMIT_BE(reg_compares, 1, X_FORM((3u << 2) | 1u, 5, 6, 32, 0)); /* cmpld cr3,r5,r6 */
    ppu_state_init(&state, reg_compares, sizeof(reg_compares), 0);
    state.gpr[3] = UINT64_MAX;
    state.gpr[4] = 1;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ((state.cr >> 23) & 1u) == 1u,
                "cmp compares signed low words into selected CR field");
    state.gpr[5] = 0x100000000ull;
    state.gpr[6] = 1;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ((state.cr >> 18) & 1u) == 1u,
                "cmpld compares unsigned 64-bit operands");

    static uint8_t logical[20];
    EMIT_BE(logical, 0, D_FORM(25, 3, 4, 2));
    EMIT_BE(logical, 1, D_FORM(26, 3, 5, 1));
    EMIT_BE(logical, 2, D_FORM(27, 3, 6, 1));
    EMIT_BE(logical, 3, D_FORM(28, 3, 7, 1));
    EMIT_BE(logical, 4, D_FORM(29, 3, 8, 1));
    ppu_state_init(&state, logical, sizeof(logical), 0);
    state.gpr[3] = 0x1000000000000001ull;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[4] == 0x1000000000020001ull,
                "oris applies unsigned immediate to high half of word");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0x1000000000000000ull,
                "xori performs register/immediate XOR");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == 0x1000000000010001ull,
                "xoris applies immediate XOR to high half of word");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == 1 &&
                ((state.cr >> 30) & 1u) == 1u,
                "andi. updates result and CR0 greater-than");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == 0 &&
                ((state.cr >> 29) & 1u) == 1u,
                "andis. shifts immediate and sets CR0 EQ for zero result");

    static uint8_t rotate[12];
    EMIT_BE(rotate, 0, ROT_FORM(21, 3, 4, 8, 24, 7, 0)); /* rlwinm wrap mask */
    EMIT_BE(rotate, 1, ROT_FORM(20, 3, 5, 0, 0, 7, 1));  /* rlwimi. */
    EMIT_BE(rotate, 2, ROT_FORM(23, 3, 6, 7, 0, 31, 0)); /* rlwnm */
    ppu_state_init(&state, rotate, sizeof(rotate), 0);
    state.gpr[3] = 0x12345678;
    state.gpr[5] = 0xaabbccdd;
    state.gpr[7] = 4;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[4] == 0x34000012,
                "rlwinm rotates a word and applies a wrapping Power bit mask");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0x12bbccdd &&
                ((state.cr >> 30) & 1u) == 1u,
                "rlwimi inserts selected rotated bits and records CR0");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == 0x23456781,
                "rlwnm takes its shift amount from RB");

    static uint8_t double_rotate_code[16];
    EMIT_BE(double_rotate_code, 0, MD_FORM(3, 4, 0, 32, 0, 1)); /* rldicl. */
    EMIT_BE(double_rotate_code, 1, MD_FORM(3, 5, 4, 15, 1, 0)); /* rldicr */
    EMIT_BE(double_rotate_code, 2, MD_FORM(6, 7, 8, 16, 2, 0)); /* rldic */
    EMIT_BE(double_rotate_code, 3, MD_FORM(8, 9, 48, 8, 3, 0)); /* rldimi */
    ppu_state_init(&state, double_rotate_code, sizeof(double_rotate_code), 0);
    state.gpr[3] = 0x123456789abcdef0ull;
    state.gpr[6] = 0xff; state.gpr[8] = 0xab; state.gpr[9] = UINT64_MAX;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[4] == 0x000000009abcdef0ull &&
                ((state.cr >> 28) & 0x0fu) == 4u,
                "rldicl rotates a doubleword, clears by Power bit numbering, and records CR0");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0x2345000000000000ull,
                "rldicr clears the right side of the rotated doubleword");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == 0xff00,
                "rldic rotates and clears both sides of a doubleword");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[9] == 0xffabffffffffffffull,
                "rldimi inserts a rotated bit field and preserves other destination bits");

    static uint8_t xform[20];
    EMIT_BE(xform, 0, X_FORM(5, 3, 4, 266, 0)); /* add r5,r3,r4 */
    EMIT_BE(xform, 1, X_FORM(6, 3, 4, 40, 0));  /* subf r6,r3,r4 */
    EMIT_BE(xform, 2, X_FORM(7, 3, 4, 28, 0));  /* and r7,r3,r4 */
    EMIT_BE(xform, 3, X_FORM(8, 3, 4, 316, 0)); /* xor r8,r3,r4 */
    EMIT_BE(xform, 4, X_FORM(9, 3, 4, 444, 1)); /* or. r9,r3,r4 */
    ppu_state_init(&state, xform, sizeof(xform), 0);
    state.gpr[3] = 0x35;
    state.gpr[4] = 0x17;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0x4c,
                "X-form add");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == (uint64_t)-0x1e,
                "X-form subtract-from operand order");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == 0x15,
                "X-form and");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == 0x22,
                "X-form xor");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[9] == 0x37 &&
                ((state.cr >> 30) & 1u) == 1u,
                "X-form or record updates CR0");

    static uint8_t bit_ops[24];
    EMIT_BE(bit_ops, 0, X_FORM(5, 3, 4, 60, 0));  /* andc */
    EMIT_BE(bit_ops, 1, X_FORM(6, 3, 4, 124, 0)); /* nor */
    EMIT_BE(bit_ops, 2, X_FORM(7, 3, 4, 284, 0)); /* eqv */
    EMIT_BE(bit_ops, 3, X_FORM(8, 3, 4, 412, 0)); /* orc */
    EMIT_BE(bit_ops, 4, X_FORM(9, 3, 4, 476, 0)); /* nand */
    EMIT_BE(bit_ops, 5, X_FORM(10, 3, 0, 104, 0));/* neg */
    ppu_state_init(&state, bit_ops, sizeof(bit_ops), 0);
    state.gpr[3] = 0xf0f0; state.gpr[4] = 0x0ff0;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0xf000,
                "andc clears bits selected by the second operand");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == 0xffffffffffff000full,
                "nor complements the OR result");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == 0xffffffffffff00ffull,
                "eqv computes bitwise equivalence");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == 0xfffffffffffff0ffull,
                "orc sets bits not selected by the second operand");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[9] == 0xffffffffffffff0full,
                "nand complements the AND result");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[10] == 0xffffffffffff0f10ull,
                "negate operates modulo the 64-bit register width");

    static uint8_t divide[16];
    EMIT_BE(divide, 0, X_FORM(5, 3, 4, 491, 0)); /* divw */
    EMIT_BE(divide, 1, X_FORM(6, 3, 4, 459, 0)); /* divwu */
    EMIT_BE(divide, 2, X_FORM(7, 3, 4, 489, 0)); /* divd */
    EMIT_BE(divide, 3, X_FORM(8, 3, 4, 457, 0)); /* divdu */
    ppu_state_init(&state, divide, sizeof(divide), 0);
    state.gpr[3] = 17; state.gpr[4] = (uint64_t)-3;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == (uint64_t)-5,
                "divw performs signed 32-bit division");
    state.gpr[3] = 20; state.gpr[4] = 3;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == 6,
                "divwu performs unsigned 32-bit division");
    state.gpr[3] = (uint64_t)-100; state.gpr[4] = 7;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == (uint64_t)-14,
                "divd performs signed 64-bit division");
    state.gpr[3] = 100; state.gpr[4] = 7;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == 14,
                "divdu performs unsigned 64-bit division");
    ppu_state_init(&state, divide, sizeof(divide), 0);
    state.gpr[3] = 10; state.gpr[4] = 0;
    ok &= check(ppu_step(&state) == PPU_STEP_UNSUPPORTED && state.pc == 0,
                "division by zero is rejected without advancing the PPU");

    static uint8_t word_ops[24];
    EMIT_BE(word_ops, 0, X_FORM(5, 3, 4, 24, 0));  /* slw */
    EMIT_BE(word_ops, 1, X_FORM(6, 3, 4, 536, 0)); /* srw */
    EMIT_BE(word_ops, 2, X_FORM(7, 3, 0, 26, 0));  /* cntlzw */
    EMIT_BE(word_ops, 3, X_FORM(8, 3, 0, 954, 0)); /* extsb */
    EMIT_BE(word_ops, 4, X_FORM(9, 3, 0, 922, 0)); /* extsh */
    EMIT_BE(word_ops, 5, X_FORM(10, 3, 0, 986, 0));/* extsw */
    ppu_state_init(&state, word_ops, sizeof(word_ops), 0);
    state.gpr[3] = 0x80000001u;
    state.gpr[4] = 1;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 2,
                "slw shifts the low word and zero-extends");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == 0x40000000,
                "srw performs a logical low-word shift");
    state.pc = 0;
    state.gpr[4] = 32;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0 &&
                ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == 0,
                "word shifts produce zero when the shift amount is at least 32");
    state.gpr[3] = 0x00100000;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == 11,
                "cntlzw counts leading zero bits in a word");
    state.gpr[3] = 0x80;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == (uint64_t)-128,
                "extsb sign-extends the low byte");
    state.gpr[3] = 0x8001;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[9] == (uint64_t)-32767,
                "extsh sign-extends the low halfword");
    state.gpr[3] = 0x80000001u;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[10] == (uint64_t)(int64_t)(int32_t)0x80000001u,
                "extsw sign-extends the low word");

    static uint8_t bit_count_code[16];
    EMIT_BE(bit_count_code, 0, X_FORM(5, 3, 0, 58, 0));  /* cntlzd */
    EMIT_BE(bit_count_code, 1, X_FORM(6, 4, 0, 122, 0)); /* popcntb */
    EMIT_BE(bit_count_code, 2, X_FORM(7, 4, 0, 378, 0)); /* popcntw */
    EMIT_BE(bit_count_code, 3, X_FORM(8, 4, 0, 506, 0)); /* popcntd */
    ppu_state_init(&state, bit_count_code, sizeof(bit_count_code), 0);
    state.gpr[3] = 0x0000000100000000ull;
    state.gpr[4] = 0x01020304ff000001ull;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 31,
                "cntlzd counts leading zeros in a 64-bit register");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == 0x0101020108000001ull,
                "popcntb counts set bits independently in each byte");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == 9,
                "popcntw counts set bits in the low word");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == 14,
                "popcntd counts set bits in the full doubleword");

    static uint8_t arithmetic_shift_code[16];
    EMIT_BE(arithmetic_shift_code, 0, X_FORM(5, 6, 2, 824, 0)); /* srawi */
    EMIT_BE(arithmetic_shift_code, 1, X_FORM(5, 7, 4, 792, 0)); /* sraw */
    EMIT_BE(arithmetic_shift_code, 2, X_FORM(5, 8, 2, 824, 1)); /* srawi. */
    EMIT_BE(arithmetic_shift_code, 3, X_FORM(5, 9, 0, 824, 0)); /* srawi by zero */
    ppu_state_init(&state, arithmetic_shift_code, sizeof(arithmetic_shift_code), 0);
    state.gpr[5] = (uint64_t)-16; state.gpr[4] = 2;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == (uint64_t)-4 &&
                state.xer_ca == 0,
                "srawi sign-fills the word and leaves CA clear when discarded bits are zero");
    state.gpr[5] = (uint64_t)-17;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == (uint64_t)-5 &&
                state.xer_ca == 1,
                "sraw sets CA when negative one bits are shifted out");
    state.gpr[5] = (uint64_t)-1;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == UINT64_MAX &&
                state.xer_ca == 1 && ((state.cr >> 28) & 0x0fu) == 8u,
                "srawi record form updates CR0 and XER CA");
    state.gpr[5] = 8;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[9] == 8 && state.xer_ca == 0,
                "zero-distance srawi clears XER CA");

    static uint8_t double_shift_code[20];
    EMIT_BE(double_shift_code, 0, X_FORM(5, 6, 4, 27, 0));  /* sld */
    EMIT_BE(double_shift_code, 1, X_FORM(5, 7, 4, 539, 0)); /* srd */
    EMIT_BE(double_shift_code, 2, X_FORM(5, 8, 4, 794, 0)); /* srad */
    EMIT_BE(double_shift_code, 3, X_FORM(5, 9, 4, 539, 0)); /* srd shift >=64 */
    EMIT_BE(double_shift_code, 4, X_FORM(5, 10, 4, 794, 0));/* srad shift >=64 */
    ppu_state_init(&state, double_shift_code, sizeof(double_shift_code), 0);
    state.gpr[5] = 0x8000000000000001ull; state.gpr[4] = 1;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == 2,
                "sld performs a 64-bit logical left shift");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == 0x4000000000000000ull,
                "srd performs a 64-bit logical right shift");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == 0xc000000000000000ull &&
                state.xer_ca == 1,
                "srad sign-fills and records shifted-out negative bits in CA");
    state.gpr[4] = 64;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[9] == 0,
                "srd returns zero when the shift amount is at least 64");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[10] == UINT64_MAX &&
                state.xer_ca == 1,
                "srad returns the sign fill and sets CA for shifts of 64 or more");

    static uint8_t multiply[12];
    EMIT_BE(multiply, 0, D_FORM(7, 4, 3, -3));        /* mulli */
    EMIT_BE(multiply, 1, X_FORM(5, 3, 4, 233, 0));   /* mulld */
    EMIT_BE(multiply, 2, X_FORM(6, 3, 4, 235, 0));   /* mullw */
    ppu_state_init(&state, multiply, sizeof(multiply), 0);
    state.gpr[3] = 7;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[4] == (uint64_t)-21,
                "mulli multiplies by a signed immediate");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == (uint64_t)-147,
                "mulld keeps the low 64 bits of the product");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == (uint32_t)(7u * (uint32_t)-21),
                "mullw keeps and zero-extends the low 32-bit product");

    static uint8_t high_multiply_code[16];
    EMIT_BE(high_multiply_code, 0, X_FORM(5, 3, 4, 9, 0));  /* mulhdu */
    EMIT_BE(high_multiply_code, 1, X_FORM(6, 3, 4, 73, 0)); /* mulhd */
    EMIT_BE(high_multiply_code, 2, X_FORM(7, 3, 4, 11, 0)); /* mulhwu */
    EMIT_BE(high_multiply_code, 3, X_FORM(8, 3, 4, 75, 0)); /* mulhw */
    ppu_state_init(&state, high_multiply_code, sizeof(high_multiply_code), 0);
    state.gpr[3] = 0x8000000000000000ull; state.gpr[4] = 2;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 1,
                "mulhdu returns the high half of an unsigned 128-bit product");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == UINT64_MAX,
                "mulhd returns the high half of a signed 128-bit product");
    ppu_state_init(&state, high_multiply_code, sizeof(high_multiply_code), 0);
    state.gpr[3] = UINT64_MAX; state.gpr[4] = 3;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 2,
                "mulhdu accounts for cross-products between operand halves");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == UINT64_MAX,
                "mulhd applies signed correction to the unsigned high product");
    ppu_state_init(&state, high_multiply_code, sizeof(high_multiply_code), 0);
    state.pc = 8;
    state.gpr[3] = 0x80000000u; state.gpr[4] = 2;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == 1,
                "mulhwu returns the high half of an unsigned word product");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == UINT64_MAX,
                "mulhw sign-extends the high half of a signed word product");
    ppu_state_init(&state, high_multiply_code, sizeof(high_multiply_code), 0);
    state.pc = 8;
    state.gpr[3] = 0x7fffffffu; state.gpr[4] = 0x7fffffffu;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == 0x3fffffff,
                "mulhwu computes the upper half across nontrivial word operands");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == 0x3fffffff,
                "mulhw computes the signed upper half for positive operands");

    static uint8_t isel_code[12];
    EMIT_BE(isel_code, 0, ISEL_FORM(5, 3, 4, 0));
    EMIT_BE(isel_code, 1, ISEL_FORM(6, 3, 4, 2));
    EMIT_BE(isel_code, 2, ISEL_FORM(7, 0, 4, 0));
    ppu_state_init(&state, isel_code, sizeof(isel_code), 0);
    state.gpr[3] = 0x1234; state.gpr[4] = 0x5678; state.cr = 0x80000000u;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0x1234,
                "isel selects RA when the selected CR bit is set");
    state.cr = 0;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == 0x5678,
                "isel selects RB when the selected CR bit is clear");
    state.cr = 0x80000000u;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == 0,
                "isel treats RA=r0 as a literal zero source");

    static uint8_t carry_ops[32];
    EMIT_BE(carry_ops, 0, X_FORM(5, 3, 4, 10, 0));  /* addc */
    EMIT_BE(carry_ops, 1, X_FORM(6, 3, 4, 138, 0)); /* adde */
    EMIT_BE(carry_ops, 2, X_FORM(7, 3, 4, 8, 0));   /* subfc */
    EMIT_BE(carry_ops, 3, X_FORM(8, 3, 4, 136, 0)); /* subfe */
    EMIT_BE(carry_ops, 4, X_FORM(9, 3, 0, 202, 0)); /* addze */
    EMIT_BE(carry_ops, 5, X_FORM(10, 3, 0, 234, 0)); /* addme */
    EMIT_BE(carry_ops, 6, X_FORM(11, 3, 0, 200, 0)); /* subfze */
    EMIT_BE(carry_ops, 7, X_FORM(12, 3, 0, 232, 0)); /* subfme */
    ppu_state_init(&state, carry_ops, sizeof(carry_ops), 0);
    state.gpr[3] = UINT64_MAX; state.gpr[4] = 1;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0 && state.xer_ca == 1,
                "addc updates XER carry on unsigned overflow");
    state.gpr[3] = 5; state.gpr[4] = 6;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == 12 && state.xer_ca == 0,
                "adde consumes the incoming XER carry");
    state.gpr[3] = 7; state.gpr[4] = 5;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == (uint64_t)-2 && state.xer_ca == 0,
                "subfc sets carry to indicate no borrow");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == (uint64_t)-3 && state.xer_ca == 0,
                "subfe consumes carry as the inverted borrow input");
    state.xer_ca = 1; state.gpr[3] = UINT64_MAX;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[9] == 0 && state.xer_ca == 1,
                "addze adds the carry bit to its register operand");
    state.gpr[3] = 0; state.xer_ca = 0;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[10] == UINT64_MAX && state.xer_ca == 0,
                "addme adds carry and minus one with correct carry-out");
    state.gpr[3] = 0; state.xer_ca = 1;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[11] == 0 && state.xer_ca == 1,
                "subfze adds the complemented operand and carry");
    state.gpr[3] = 0; state.xer_ca = 0;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[12] == (uint64_t)-2 && state.xer_ca == 1,
                "subfme subtracts from minus one with correct carry-out");

    static uint8_t fp_code[92];
    EMIT_BE(fp_code, 0, FP_A(3, 1, 2, 0, 21)); /* fadd */
    EMIT_BE(fp_code, 1, FP_A(4, 1, 2, 0, 20)); /* fsub */
    EMIT_BE(fp_code, 2, FP_A(5, 1, 0, 2, 25)); /* fmul */
    EMIT_BE(fp_code, 3, FP_A(6, 2, 1, 0, 18)); /* fdiv */
    EMIT_BE(fp_code, 4, FP_X(7, 2, 72));  /* fmr */
    EMIT_BE(fp_code, 5, FP_X(8, 9, 264)); /* fabs */
    EMIT_BE(fp_code, 6, FP_X(10, 9, 40)); /* fneg */
    EMIT_BE(fp_code, 7, FP_X(11, 9, 136));/* fnabs */
    EMIT_BE(fp_code, 8, FP_S(12, 1, 2, 0, 21)); /* fadds */
    EMIT_BE(fp_code, 9, FP_S(13, 1, 2, 0, 20)); /* fsubs */
    EMIT_BE(fp_code, 10, FP_S(14, 1, 0, 2, 25)); /* fmuls */
    EMIT_BE(fp_code, 11, FP_S(15, 2, 1, 0, 18)); /* fdivs */
    EMIT_BE(fp_code, 12, FP_X(16, 1, 12)); /* frsp */
    EMIT_BE(fp_code, 13, FP_A(17, 1, 2, 9, 23)); /* fsel */
    EMIT_BE(fp_code, 14, FP_A(18, 1, 2, 9, 23)); /* fsel */
    EMIT_BE(fp_code, 15, FP_X(19, 1, 392)); /* frin */
    EMIT_BE(fp_code, 16, FP_X(20, 1, 424)); /* friz */
    EMIT_BE(fp_code, 17, FP_X(21, 1, 456)); /* frip */
    EMIT_BE(fp_code, 18, FP_X(22, 1, 488)); /* frim */
    EMIT_BE(fp_code, 19, FP_X(23, 1, 456)); /* frip negative */
    EMIT_BE(fp_code, 20, FP_X(24, 1, 488)); /* frim negative */
    EMIT_BE(fp_code, 21, FP_X(25, 1, 392)); /* frin tie to even */
    EMIT_BE(fp_code, 22, FP_X(26, 1, 392)); /* frin tie to even */
    ppu_state_init(&state, fp_code, sizeof(fp_code), 0);
    state.fpr[1] = -1.5; state.fpr[2] = 2.25; state.fpr[9] = -3.5;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[3] == 0.75,
                "fadd adds double-precision FPR operands");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[4] == -3.75,
                "fsub subtracts double-precision FPR operands");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[5] == -3.375,
                "fmul reads its third FPR operand from FRC");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[6] == -1.5,
                "fdiv divides double-precision FPR operands");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[7] == 2.25,
                "fmr copies an FPR value");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[8] == 3.5,
                "fabs clears the FPR sign bit");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[10] == 3.5,
                "fneg toggles the FPR sign bit");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[11] == -3.5,
                "fnabs sets the FPR sign bit");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[12] == 0.75,
                "fadds rounds arithmetic to single precision");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[13] == -3.75,
                "fsubs rounds arithmetic to single precision");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[14] == -3.375,
                "fmuls reads its third single-precision operand from FRC");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[15] == -1.5,
                "fdivs divides single-precision FPR operands");
    state.fpr[1] = 1.0 / 3.0;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[16] == (double)(float)(1.0 / 3.0),
                "frsp rounds its source to single precision");
    state.fpr[1] = -1.0; state.fpr[2] = 2.0; state.fpr[9] = 3.0;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[17] == 2.0,
                "fsel chooses FRB when FRA is negative");
    state.fpr[1] = 0.0;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[18] == 3.0,
                "fsel chooses FRC when FRA is zero");
    state.fpr[1] = 1.25;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[19] == 1.0,
                "frin rounds to nearest integer");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[20] == 1.0,
                "friz rounds toward zero");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[21] == 2.0,
                "frip rounds toward positive infinity");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[22] == 1.0,
                "frim rounds positive values toward negative infinity");
    state.fpr[1] = -1.25;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[23] == -1.0,
                "frip rounds negative values toward positive infinity");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[24] == -2.0,
                "frim rounds negative values toward negative infinity");
    state.fpr[1] = 2.5;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[25] == 2.0,
                "frin breaks an exact half tie toward the even integer");
    state.fpr[1] = 3.5;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[26] == 4.0,
                "frin rounds an odd half tie to the even integer");

    static uint8_t fctiwz_code[8];
    static uint8_t fctiwz_memory[8];
    EMIT_BE(fctiwz_code, 0, FP_X(3, 1, 15)); /* fctiwz f3,f1 */
    EMIT_BE(fctiwz_code, 1, D_FORM(54, 3, 4, 0)); /* stfd f3,0(r4) */
    ppu_state_init(&state, fctiwz_code, sizeof(fctiwz_code), 0);
    ppu_set_memory(&state, fctiwz_memory, sizeof(fctiwz_memory));
    state.gpr[4] = 0; state.fpr[1] = -123.75;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr_integer_word_valid[3] &&
                state.fpr_integer_word[3] == 0xffffff85,
                "fctiwz converts toward zero and marks its raw integer-word format");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && fctiwz_memory[4] == 0xff &&
                fctiwz_memory[7] == 0x85,
                "stfd preserves the fctiwz word in the low half of FPR storage");
    ppu_state_init(&state, fctiwz_code, sizeof(fctiwz_code), 0);
    state.fpr[1] = 2147483648.0;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr_integer_word[3] == 0x7fffffffu,
                "fctiwz saturates a positive out-of-range input");

    static uint8_t fp_compare[16];
    EMIT_BE(fp_compare, 0, (63u << 26) | (2u << 23) | (1u << 16) | (2u << 11)); /* fcmpu cr2 */
    EMIT_BE(fp_compare, 1, (63u << 26) | (4u << 23) | (1u << 16) | (2u << 11)); /* fcmpu cr4 */
    EMIT_BE(fp_compare, 2, (63u << 26) | (5u << 23) | (1u << 16) | (2u << 11)); /* fcmpu cr5 */
    EMIT_BE(fp_compare, 3, (63u << 26) | (6u << 23) | (1u << 16) | (2u << 11) | (32u << 1)); /* fcmpo cr6 */
    ppu_state_init(&state, fp_compare, sizeof(fp_compare), 0);
    state.fpr[1] = 1.25; state.fpr[2] = 2.5;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ((state.cr >> 23) & 1u) == 1u,
                "fcmpu writes floating-point less-than to selected CR field");
    state.fpr[1] = state.fpr[2];
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ((state.cr >> 13) & 1u) == 1u,
                "fcmpu writes floating-point equality to selected CR field");
    union { uint64_t bits; double value; } quiet_nan = { 0x7ff8000000000001ull };
    state.fpr[1] = quiet_nan.value;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ((state.cr >> 8) & 0x0fu) == 1u,
                "fcmpu writes the unordered condition for NaN");
    state.pc = 12;
    ok &= check(ppu_step(&state) == PPU_STEP_UNSUPPORTED && state.pc == 12,
                "fcmpo with NaN is rejected until floating-point exceptions are modeled");

    static uint8_t fp_memory_code[24];
    EMIT_BE(fp_memory_code, 0, D_FORM(48, 3, 4, 0));  /* lfs */
    EMIT_BE(fp_memory_code, 1, D_FORM(50, 4, 4, 8));  /* lfd */
    EMIT_BE(fp_memory_code, 2, D_FORM(52, 5, 4, 4));  /* stfs */
    EMIT_BE(fp_memory_code, 3, D_FORM(54, 6, 4, 16)); /* stfd */
    EMIT_BE(fp_memory_code, 4, D_FORM(49, 7, 8, 0));  /* lfsu */
    EMIT_BE(fp_memory_code, 5, D_FORM(55, 6, 8, 8));  /* stfdu */
    static uint8_t fp_memory[40] = { 0x3f, 0xc0, 0, 0, 0, 0, 0, 0,
                              0x40, 0x02, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0,
                              0xc0, 0x20, 0, 0 };
    ppu_state_init(&state, fp_memory_code, sizeof(fp_memory_code), 0);
    ppu_set_memory(&state, fp_memory, sizeof(fp_memory));
    state.gpr[4] = 0; state.gpr[8] = 24;
    state.fpr[5] = -3.5; state.fpr[6] = 7.25;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[3] == 1.5,
                "lfs loads and widens a big-endian single value");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[4] == 2.25,
                "lfd loads a big-endian double value");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && fp_memory[4] == 0xc0 && fp_memory[7] == 0,
                "stfs rounds and stores a big-endian single value");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && fp_memory[16] == 0x40 && fp_memory[17] == 0x1d,
                "stfd stores a big-endian double value");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[7] == -2.5 && state.gpr[8] == 24,
                "lfsu loads a single value and updates the base");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == 32 && fp_memory[32] == 0x40,
                "stfdu stores a double value and updates the base");

    static uint8_t fp_indexed_code[28];
    EMIT_BE(fp_indexed_code, 0, X_FORM(3, 4, 5, 535, 0)); /* lfsx */
    EMIT_BE(fp_indexed_code, 1, X_FORM(6, 4, 5, 567, 0)); /* lfsux */
    EMIT_BE(fp_indexed_code, 2, X_FORM(7, 4, 8, 599, 0)); /* lfdx */
    EMIT_BE(fp_indexed_code, 3, X_FORM(9, 4, 8, 663, 0)); /* stfsx */
    EMIT_BE(fp_indexed_code, 4, X_FORM(10, 4, 8, 695, 0)); /* stfsux */
    EMIT_BE(fp_indexed_code, 5, X_FORM(11, 4, 8, 727, 0)); /* stfdx */
    EMIT_BE(fp_indexed_code, 6, X_FORM(12, 4, 8, 759, 0)); /* stfdux */
    static uint8_t fp_indexed_memory[48] = { 0 };
    fp_indexed_memory[4] = 0x3f; fp_indexed_memory[5] = 0xc0;
    fp_indexed_memory[8] = 0x40; fp_indexed_memory[9] = 0x02;
    ppu_state_init(&state, fp_indexed_code, sizeof(fp_indexed_code), 0);
    ppu_set_memory(&state, fp_indexed_memory, sizeof(fp_indexed_memory));
    state.gpr[5] = 4; state.gpr[8] = 8;
    state.fpr[9] = -1.5; state.fpr[10] = 2.5;
    state.fpr[11] = 7.25; state.fpr[12] = -9.5;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[3] == 1.5,
                "lfsx loads an indexed big-endian single value");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[6] == 1.5 && state.gpr[4] == 4,
                "lfsux loads single precision and updates its GPR base");
    state.gpr[4] = 0;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.fpr[7] == 2.25,
                "lfdx loads an indexed big-endian double value");
    const PpuStepResult stfsx_result = ppu_step(&state);
    ok &= check(stfsx_result == PPU_STEP_OK,
                "stfsx executes as an indexed store");
    ok &= check(fp_indexed_memory[8] == 0xbf && fp_indexed_memory[9] == 0xc0,
                "stfsx rounds and stores an indexed single value");
    state.gpr[4] = 4;
    const PpuStepResult stfsux_result = ppu_step(&state);
    ok &= check(stfsux_result == PPU_STEP_OK && state.gpr[4] == 12,
                "stfsux executes and updates its GPR base");
    ok &= check(fp_indexed_memory[12] == 0x40 && fp_indexed_memory[13] == 0x20,
                "stfsux stores the indexed single value");
    state.gpr[4] = 24;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && fp_indexed_memory[32] == 0x40 &&
                fp_indexed_memory[39] == 0x00,
                "stfdx stores an indexed big-endian double value");
    state.gpr[4] = 32;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[4] == 40 &&
                fp_indexed_memory[40] == 0xc0 && fp_indexed_memory[47] == 0x00,
                "stfdux stores double precision and updates its GPR base");

    static uint8_t spr_code[24];
    EMIT_BE(spr_code, 0, SPR_FORM(4, 8, 339)); /* mflr r4 */
    EMIT_BE(spr_code, 1, SPR_FORM(5, 9, 339)); /* mfctr r5 */
    EMIT_BE(spr_code, 2, SPR_FORM(6, 8, 467)); /* mtlr r6 */
    EMIT_BE(spr_code, 3, SPR_FORM(7, 9, 467)); /* mtctr r7 */
    EMIT_BE(spr_code, 4, SPR_FORM(8, 1, 339)); /* mfxer r8 */
    EMIT_BE(spr_code, 5, SPR_FORM(9, 1, 467)); /* mtxer r9 */
    ppu_state_init(&state, spr_code, sizeof(spr_code), 0);
    state.lr = 0x1234;
    state.ctr = 0x5678;
    state.gpr[6] = 0x9abc;
    state.gpr[7] = 0xdef0;
    state.xer_so = 1; state.xer_ov = 1; state.xer_ca = 0; state.xer_bc = 0x35;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[4] == 0x1234,
                "mfspr reads LR");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0x5678,
                "mfspr reads CTR");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.lr == 0x9abc,
                "mtspr writes LR");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.ctr == 0xdef0,
                "mtspr writes CTR");
    ok &= check(ppu_step(&state) == PPU_STEP_OK &&
                state.gpr[8] == (1ull << 31 | 1ull << 30 | 0x35ull),
                "mfspr reads XER summary, carry, and byte-count fields");
    state.gpr[9] = (1ull << 31) | (1ull << 30) | 0x2a;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.xer_so == 1 &&
                state.xer_ov == 1 && state.xer_ca == 0 && state.xer_bc == 0x2a,
                "mtspr restores modeled XER fields");

    static uint8_t cr_code[12];
    EMIT_BE(cr_code, 0, X_FORM(4, 0, 0, 19, 0)); /* mfcr r4 */
    EMIT_BE(cr_code, 1, (31u << 26) | (5u << 21) | (0xffu << 12) | (144u << 1)); /* mtcrf all */
    EMIT_BE(cr_code, 2, (31u << 26) | (6u << 21) | (0x20u << 12) | (144u << 1)); /* mtcrf CR2 */
    ppu_state_init(&state, cr_code, sizeof(cr_code), 0);
    state.cr = 0x12345678;
    state.gpr[5] = 0x89abcdef;
    state.gpr[6] = 0x00f00000;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[4] == 0x12345678,
                "mfcr copies the condition register to a GPR");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.cr == 0x89abcdef,
                "mtcrf restores all CR fields");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.cr == 0x89fbcdef,
                "mtcrf updates only the selected CR field");

    static uint8_t mcrf_code[4];
    EMIT_BE(mcrf_code, 0, (19u << 26) | (2u << 23) | (4u << 18)); /* mcrf cr2,cr4 */
    ppu_state_init(&state, mcrf_code, sizeof(mcrf_code), 0);
    state.cr = 0x12345678;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.cr == 0x12545678,
                "mcrf copies one condition-register field and preserves the others");
    static uint8_t mcrxr_code[4];
    EMIT_BE(mcrxr_code, 0, (31u << 26) | (5u << 23) | (512u << 1)); /* mcrxr cr5 */
    ppu_state_init(&state, mcrxr_code, sizeof(mcrxr_code), 0);
    state.xer_so = 1; state.xer_ov = 0; state.xer_ca = 1; state.xer_bc = 6;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ((state.cr >> 8) & 0x0fu) == 0x0au &&
                state.xer_so == 0 && state.xer_ov == 0 && state.xer_ca == 0 && state.xer_bc == 6,
                "mcrxr transfers XER summary bits to CR and clears them");

    static uint8_t cr_logic[16];
    EMIT_BE(cr_logic, 0, CR_FORM(3, 1, 2, 257)); /* crand 3,1,2 */
    EMIT_BE(cr_logic, 1, CR_FORM(4, 1, 2, 193)); /* crxor 4,1,2 */
    EMIT_BE(cr_logic, 2, CR_FORM(5, 3, 4, 33));  /* crnor 5,3,4 */
    EMIT_BE(cr_logic, 3, CR_FORM(6, 3, 4, 289)); /* creqv 6,3,4 */
    ppu_state_init(&state, cr_logic, sizeof(cr_logic), 0);
    state.cr = (1u << (31u - 1u)) | (1u << (31u - 2u));
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ((state.cr >> (31u - 3u)) & 1u) == 1u,
                "crand combines selected CR bits");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ((state.cr >> (31u - 4u)) & 1u) == 0u,
                "crxor writes the selected condition bit");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ((state.cr >> (31u - 5u)) & 1u) == 0u,
                "crnor computes the inverse OR of CR bits");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ((state.cr >> (31u - 6u)) & 1u) == 0u,
                "creqv writes the equivalence result");

    static uint8_t ds_code[20];
    EMIT_BE(ds_code, 0, DS_FORM(58, 5, 3, 0, 0));  /* ld */
    EMIT_BE(ds_code, 1, DS_FORM(58, 6, 3, 8, 2));  /* lwa */
    EMIT_BE(ds_code, 2, DS_FORM(62, 7, 3, 16, 0)); /* std */
    EMIT_BE(ds_code, 3, DS_FORM(58, 8, 3, 8, 1));  /* ldu */
    EMIT_BE(ds_code, 4, DS_FORM(62, 7, 3, 8, 1));  /* stdu */
    static uint8_t ds_memory[32] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    ppu_state_init(&state, ds_code, sizeof(ds_code), 0);
    ppu_set_memory(&state, ds_memory, sizeof(ds_memory));
    state.gpr[3] = 0;
    state.gpr[7] = 0xfedcba9876543210ull;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0x0123456789abcdefull,
                "ld reads a big-endian 64-bit value");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == UINT64_MAX,
                "lwa sign-extends a big-endian word");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && ds_memory[16] == 0xfe && ds_memory[23] == 0x10,
                "std writes a big-endian 64-bit value");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == UINT64_MAX && state.gpr[3] == 8,
                "ldu reads and updates its base register");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[3] == 16 && ds_memory[16] == 0xfe,
                "stdu writes and updates its base register");

    static uint8_t indexed_code[24];
    EMIT_BE(indexed_code, 0, X_FORM(5, 3, 4, 23, 0));    /* lwzx */
    EMIT_BE(indexed_code, 1, X_FORM(6, 3, 7, 343, 0));   /* lhax */
    EMIT_BE(indexed_code, 2, X_FORM(8, 3, 9, 21, 0));    /* ldx */
    EMIT_BE(indexed_code, 3, X_FORM(10, 3, 11, 151, 0)); /* stwx */
    EMIT_BE(indexed_code, 4, X_FORM(12, 3, 13, 53, 0));  /* ldux */
    EMIT_BE(indexed_code, 5, X_FORM(14, 3, 15, 181, 0)); /* stdux */
    static uint8_t indexed_memory[40] = { 0, 0, 0, 0, 0x12, 0x34, 0x56, 0x78,
                                   0x80, 0x01, 0, 0, 0, 0, 0, 0,
                                   0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef };
    ppu_state_init(&state, indexed_code, sizeof(indexed_code), 0);
    ppu_set_memory(&state, indexed_memory, sizeof(indexed_memory));
    state.gpr[4] = 4; state.gpr[7] = 8; state.gpr[9] = 16;
    state.gpr[11] = 24; state.gpr[13] = 16; state.gpr[15] = 16;
    state.gpr[10] = 0x89abcdefu; state.gpr[14] = 0xfedcba9876543210ull;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0x12345678,
                "lwzx computes RA plus RB and loads a big-endian word");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == (uint64_t)-0x7fff,
                "lhax sign-extends an indexed halfword");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == 0x0123456789abcdefull,
                "ldx loads an indexed doubleword");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && indexed_memory[24] == 0x89 && indexed_memory[27] == 0xef,
                "stwx stores an indexed word");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[12] == 0x0123456789abcdefull && state.gpr[3] == 16,
                "ldux loads and writes back the effective address");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[3] == 32 && indexed_memory[32] == 0xfe,
                "stdux stores an indexed doubleword and updates its base");

    static uint8_t halfword_code[12];
    EMIT_BE(halfword_code, 0, D_FORM(40, 7, 0, 0)); /* lhz r7,0(0) */
    EMIT_BE(halfword_code, 1, D_FORM(42, 8, 0, 0)); /* lha r8,0(0) */
    EMIT_BE(halfword_code, 2, D_FORM(44, 9, 0, 4)); /* sth r9,4(0) */

    static uint8_t memory[12] = { 0, 0, 0, 0, 0x12, 0x34, 0x56, 0x78, 0, 0, 0, 0 };
    memory[0] = 0x80; memory[1] = 0x01;
    ppu_state_init(&state, halfword_code, sizeof(halfword_code), 0);
    ppu_set_memory(&state, memory, sizeof(memory));
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == 0x8001,
                "lhz zero-extends big-endian halfword");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == (uint64_t)-0x7fff,
                "lha sign-extends big-endian halfword");
    state.gpr[9] = 0x1abcd;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && memory[4] == 0xab && memory[5] == 0xcd,
                "sth writes low halfword in big-endian order");
    EMIT_BE(halfword_code, 0, D_FORM(40, 7, 0, 1));
    ppu_state_init(&state, halfword_code, 4, 0);
    ppu_set_memory(&state, memory, sizeof(memory));
    ok &= check(ppu_step(&state) == PPU_STEP_MEMORY_FAULT && state.pc == 0,
                "misaligned halfword load faults without advancing PC");

    static uint8_t update_code[28];
    EMIT_BE(update_code, 0, D_FORM(33, 7, 3, 0));  /* lwzu r7,0(r3) */
    EMIT_BE(update_code, 1, D_FORM(37, 5, 3, 4));  /* stwu r5,4(r3) */
    EMIT_BE(update_code, 2, D_FORM(35, 8, 3, -4)); /* lbzu r8,-4(r3) */
    EMIT_BE(update_code, 3, D_FORM(39, 9, 3, 2));  /* stbu r9,2(r3) */
    EMIT_BE(update_code, 4, D_FORM(41, 10, 3, 0)); /* lhzu r10,0(r3) */
    EMIT_BE(update_code, 5, D_FORM(43, 11, 3, 2)); /* lhau r11,2(r3) */
    EMIT_BE(update_code, 6, D_FORM(45, 12, 3, 4)); /* sthu r12,4(r3) */
    memory[4] = 0x12; memory[5] = 0x34; memory[6] = 0x56; memory[7] = 0x78;
    ppu_state_init(&state, update_code, sizeof(update_code), 0);
    ppu_set_memory(&state, memory, sizeof(memory));
    state.gpr[3] = 4;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[7] == 0x12345678 && state.gpr[3] == 4,
                "lwzu loads and writes back updated base");
    state.gpr[5] = 0x99;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[3] == 8 && memory[11] == 0x99,
                "stwu stores and writes back updated base");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[8] == 0x12 && state.gpr[3] == 4,
                "lbzu loads byte and writes back updated base");
    state.gpr[9] = 0x1aa;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && memory[6] == 0xaa && state.gpr[3] == 6,
                "stbu stores byte and writes back updated base");
    memory[6] = 0x80; memory[7] = 0x01;
    state.pc = 16; state.gpr[3] = 4;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[10] == 0x1234 && state.gpr[3] == 4,
                "lhzu zero-extends and writes back updated base");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[11] == (uint64_t)-0x7fff && state.gpr[3] == 6,
                "lhau sign-extends and writes back updated base");
    state.gpr[12] = 0x1abcd;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && memory[10] == 0xab && memory[11] == 0xcd && state.gpr[3] == 10,
                "sthu stores low halfword and writes back updated base");
    EMIT_BE(update_code, 0, D_FORM(33, 3, 3, 0));
    ppu_state_init(&state, update_code, 4, 0);
    ppu_set_memory(&state, memory, sizeof(memory));
    ok &= check(ppu_step(&state) == PPU_STEP_UNSUPPORTED && state.pc == 0,
                "lwzu with RT equal to RA is rejected");
    static uint8_t memory_code[24];
    EMIT_BE(memory_code, 0, D_FORM(14, 3, 0, 4));      /* addi r3,0,4 */
    EMIT_BE(memory_code, 1, D_FORM(32, 4, 3, 0));      /* lwz r4,0(r3) */
    EMIT_BE(memory_code, 2, D_FORM(14, 5, 0, 0x55));   /* addi r5,0,0x55 */
    EMIT_BE(memory_code, 3, D_FORM(36, 5, 0, 4));      /* stw r5,4(0) */
    EMIT_BE(memory_code, 4, D_FORM(34, 6, 0, 7));      /* lbz r6,7(0) */
    memory[4] = 0x12; memory[5] = 0x34; memory[6] = 0x56; memory[7] = 0x78;
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
    EMIT_BE(memory_code, 0, D_FORM(32, 4, 3, 1));
    ppu_state_init(&state, memory_code, 4, 0);
    ppu_set_memory(&state, memory, sizeof(memory));
    state.gpr[3] = UINT64_MAX;
    ok &= check(ppu_step(&state) == PPU_STEP_MEMORY_FAULT && state.pc == 0,
                "overflowing displacement address cannot wrap into mapped memory");
    EMIT_BE(memory_code, 0, X_FORM(4, 3, 5, 23, 0));
    ppu_state_init(&state, memory_code, 4, 0);
    ppu_set_memory(&state, memory, sizeof(memory));
    state.gpr[3] = UINT64_MAX; state.gpr[5] = 1;
    ok &= check(ppu_step(&state) == PPU_STEP_MEMORY_FAULT && state.pc == 0,
                "overflowing indexed address cannot wrap into mapped memory");

    static uint8_t multiple_code[12];
    EMIT_BE(multiple_code, 0, D_FORM(46, 29, 0, 0)); /* lmw r29,0(0) */
    EMIT_BE(multiple_code, 1, D_FORM(47, 30, 0, 4)); /* stmw r30,4(0) */
    EMIT_BE(multiple_code, 2, D_FORM(46, 30, 0, 8)); /* lmw r30,8(0), out of bounds */
    static uint8_t multiple_memory[12] = { 0x01, 0x02, 0x03, 0x04,
                                    0x11, 0x22, 0x33, 0x44,
                                    0x55, 0x66, 0x77, 0x88 };
    ppu_state_init(&state, multiple_code, sizeof(multiple_code), 0);
    ppu_set_memory(&state, multiple_memory, sizeof(multiple_memory));
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[29] == 0x01020304 &&
                state.gpr[30] == 0x11223344 && state.gpr[31] == 0x55667788,
                "lmw loads GPR RT through r31 in big-endian order");
    state.gpr[30] = 0xaabbccdd; state.gpr[31] = 0x12345678;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && multiple_memory[4] == 0xaa &&
                multiple_memory[7] == 0xdd && multiple_memory[8] == 0x12 &&
                multiple_memory[11] == 0x78,
                "stmw stores GPR RT through r31 in big-endian order");
    state.gpr[30] = 0xdeadbeef;
    ok &= check(ppu_step(&state) == PPU_STEP_MEMORY_FAULT && state.pc == 8 &&
                state.gpr[30] == 0xdeadbeef,
                "out-of-bounds lmw faults before changing any destination register");

    static uint8_t string_code[12];
    EMIT_BE(string_code, 0, X_FORM(5, 0, 4, 533, 0)); /* lswx r5,0,r4 */
    EMIT_BE(string_code, 1, X_FORM(7, 0, 4, 661, 0)); /* stswx r7,0,r4 */
    EMIT_BE(string_code, 2, X_FORM(9, 0, 4, 533, 0)); /* zero-count lswx */
    static uint8_t string_memory[20] = { 0, 0, 0, 0, 0x11, 0x22, 0x33, 0x44,
                                  0x55, 0x66, 0x77, 0x88 };
    ppu_state_init(&state, string_code, sizeof(string_code), 0);
    ppu_set_memory(&state, string_memory, sizeof(string_memory));
    state.xer_bc = 6; state.gpr[4] = 4;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0x11223344 &&
                state.gpr[6] == 0x55660000,
                "lswx loads XER byte-count bytes and clears the partial word tail");
    state.gpr[4] = 8; state.gpr[7] = 0xaabbccdd; state.gpr[8] = 0xeeff0000;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && string_memory[8] == 0xaa &&
                string_memory[13] == 0xff,
                "stswx stores XER byte-count bytes from consecutive GPRs");
    state.xer_bc = 0; state.gpr[4] = 4; state.gpr[9] = 0xabcdef01;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[9] == 0xabcdef01,
                "zero-byte lswx leaves its starting GPR unmodified");

    static uint8_t byte_reverse_code[20];
    EMIT_BE(byte_reverse_code, 0, X_FORM(5, 3, 4, 534, 0)); /* lwbrx */
    EMIT_BE(byte_reverse_code, 1, X_FORM(6, 3, 4, 790, 0)); /* lhbrx */
    EMIT_BE(byte_reverse_code, 2, X_FORM(7, 3, 8, 662, 0)); /* stwbrx */
    EMIT_BE(byte_reverse_code, 3, X_FORM(9, 3, 10, 918, 0));/* sthbrx */
    EMIT_BE(byte_reverse_code, 4, X_FORM(5, 3, 4, 534, 0)); /* misaligned lwbrx */
    static uint8_t byte_reverse_memory[16];
    for (size_t i = 0; i < sizeof(byte_reverse_memory); ++i) byte_reverse_memory[i] = 0;
    byte_reverse_memory[4] = 0x12; byte_reverse_memory[5] = 0x34;
    byte_reverse_memory[6] = 0x56; byte_reverse_memory[7] = 0x78;
    ppu_state_init(&state, byte_reverse_code, sizeof(byte_reverse_code), 0);
    ppu_set_memory(&state, byte_reverse_memory, sizeof(byte_reverse_memory));
    state.gpr[3] = 4; state.gpr[4] = 0; state.gpr[8] = 4; state.gpr[10] = 8;
    state.gpr[7] = 0xaabbccdd; state.gpr[9] = 0xabcd;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0x78563412,
                "lwbrx loads and reverses a big-endian word");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[6] == 0x3412,
                "lhbrx loads and reverses a big-endian halfword");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && byte_reverse_memory[8] == 0xdd &&
                byte_reverse_memory[11] == 0xaa,
                "stwbrx stores a word in byte-reversed order");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && byte_reverse_memory[12] == 0xcd &&
                byte_reverse_memory[13] == 0xab,
                "sthbrx stores a halfword in byte-reversed order");
    state.gpr[4] = 1;
    ok &= check(ppu_step(&state) == PPU_STEP_MEMORY_FAULT && state.pc == 16,
                "misaligned lwbrx faults without advancing PC");

    static uint8_t byte_reversed_doubleword[8];
    static uint8_t byte_reversed_doubleword_memory[48];
    EMIT_BE(byte_reversed_doubleword, 0, X_FORM(5, 3, 4, 532, 0)); /* ldbrx */
    EMIT_BE(byte_reversed_doubleword, 1, X_FORM(6, 3, 4, 660, 0)); /* stdbrx */
    ppu_state_init(&state, byte_reversed_doubleword, sizeof(byte_reversed_doubleword), 0);
    ppu_set_memory(&state, byte_reversed_doubleword_memory, sizeof(byte_reversed_doubleword_memory));
    state.gpr[3] = 32; state.gpr[4] = 0;
    state.memory[32] = 1; state.memory[33] = 2; state.memory[34] = 3; state.memory[35] = 4;
    state.memory[36] = 5; state.memory[37] = 6; state.memory[38] = 7; state.memory[39] = 8;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0x0807060504030201ull,
                "ldbrx loads and reverses a big-endian doubleword");
    state.gpr[6] = 0x1122334455667788ull;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.memory[32] == 0x88 && state.memory[39] == 0x11,
                "stdbrx stores a doubleword in byte-reversed order");

    static uint8_t elf[128] = { 0 };
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
    static uint8_t guest_memory[12];
    for (size_t i = 0; i < sizeof(guest_memory); ++i) guest_memory[i] = 0xcc;
    ok &= check(ppu_elf_load_segments(elf, sizeof(elf), &view, 0x10000,
                                      guest_memory, sizeof(guest_memory)) == PPU_ELF_OK &&
                guest_memory[0] == 0xde && guest_memory[3] == 0xef &&
                guest_memory[7] == 0xbe && guest_memory[8] == 0 && guest_memory[11] == 0,
                "ELF segment bytes copied and BSS tail zero-filled");
    ok &= check(ppu_elf_load_segments(elf, sizeof(elf), &view, 0x10000,
                                      guest_memory, sizeof(guest_memory) - 1) == PPU_ELF_BOUNDS_ERROR,
                "ELF segment outside guest memory rejected");
    view.segment_count = 2;
    view.segments[1].flags = view.segments[0].flags;
    view.segments[1].file_offset = view.segments[0].file_offset;
    view.segments[1].virtual_address = 0x10010;
    view.segments[1].file_size = view.segments[0].file_size;
    view.segments[1].memory_size = view.segments[0].memory_size;
    static uint8_t unchanged_memory[12];
    for (size_t i = 0; i < sizeof(unchanged_memory); ++i) unchanged_memory[i] = 0x5a;
    ok &= check(ppu_elf_load_segments(elf, sizeof(elf), &view, 0x10000,
                                      unchanged_memory, sizeof(unchanged_memory)) == PPU_ELF_BOUNDS_ERROR,
                "ELF later segment outside guest memory rejected during prevalidation");
    int memory_unchanged = 1;
    for (size_t i = 0; i < sizeof(unchanged_memory); ++i)
        if (unchanged_memory[i] != 0x5a) memory_unchanged = 0;
    ok &= check(memory_unchanged,
                "invalid ELF segment leaves guest memory completely unchanged");
    view.segment_count = 1;
    static uint8_t program_elf[140];
    for (size_t i = 0; i < sizeof(elf); ++i) program_elf[i] = elf[i];
    put_u64_be(program_elf + 96, 20);
    put_u64_be(program_elf + 104, 24);
    put_u32_be(program_elf + 120, D_FORM(14, 3, 0, 5));
    put_u32_be(program_elf + 124, (11u << 26) | (1u << 21) | (3u << 16) | 5u);
    put_u32_be(program_elf + 128, (16u << 26) | (12u << 21) | (2u << 16) | (2u << 2));
    put_u32_be(program_elf + 132, D_FORM(14, 4, 0, 1)); /* skipped */
    put_u32_be(program_elf + 136, D_FORM(14, 4, 0, 7));
    ok &= check(ppu_elf_parse(program_elf, sizeof(program_elf), &view) == PPU_ELF_OK,
                "synthetic executable program ELF parsed");
    static uint8_t program_memory[24];
    ok &= check(ppu_elf_load_segments(program_elf, sizeof(program_elf), &view, 0x10000,
                                      program_memory, sizeof(program_memory)) == PPU_ELF_OK,
                "synthetic executable segment loaded for PPU integration");
    ppu_state_init(&state, program_memory, sizeof(program_memory), view.entry);
    ppu_set_code_base(&state, 0x10000);
    ppu_set_memory_at(&state, program_memory, sizeof(program_memory), 0x10000);
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 0x10004 && state.gpr[3] == 5,
                "PPU fetches first instruction from ELF virtual entry address");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 0x10008 &&
                ((state.cr >> 29) & 1u) == 1u,
                "PPU executes compare from mapped ELF code");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 0x10010 && state.gpr[4] == 0,
                "PPU executes conditional branch and skips the next instruction");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.pc == 0x10014 && state.gpr[4] == 7,
                "PPU resumes at branch target in mapped ELF segment");
    ExitProcess(ok ? 0u : 1u);
}
