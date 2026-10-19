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
#define ROT_FORM(op, rs, ra, sh, mb, me, rc) (((uint32_t)(op) << 26) | \
    ((uint32_t)(rs) << 21) | ((uint32_t)(ra) << 16) | ((uint32_t)(sh) << 11) | \
    ((uint32_t)(mb) << 6) | ((uint32_t)(me) << 1) | (rc))
#define SPR_FORM(rt, spr, xo) ((31u << 26) | ((uint32_t)(rt) << 21) | \
    (((uint32_t)(spr) & 31u) << 16) | (((uint32_t)(spr) >> 5) << 11) | ((uint32_t)(xo) << 1))
#define DS_FORM(op, rt, ra, disp, xo) (((uint32_t)(op) << 26) | ((uint32_t)(rt) << 21) | \
    ((uint32_t)(ra) << 16) | ((uint16_t)(disp) & 0xfffcu) | (uint32_t)(xo))
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

    uint8_t reg_compares[8];
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

    uint8_t logical[20];
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

    uint8_t rotate[12];
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

    uint8_t xform[20];
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

    uint8_t word_ops[24];
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

    uint8_t multiply[12];
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

    uint8_t spr_code[16];
    EMIT_BE(spr_code, 0, SPR_FORM(4, 8, 339)); /* mflr r4 */
    EMIT_BE(spr_code, 1, SPR_FORM(5, 9, 339)); /* mfctr r5 */
    EMIT_BE(spr_code, 2, SPR_FORM(6, 8, 467)); /* mtlr r6 */
    EMIT_BE(spr_code, 3, SPR_FORM(7, 9, 467)); /* mtctr r7 */
    ppu_state_init(&state, spr_code, sizeof(spr_code), 0);
    state.lr = 0x1234;
    state.ctr = 0x5678;
    state.gpr[6] = 0x9abc;
    state.gpr[7] = 0xdef0;
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[4] == 0x1234,
                "mfspr reads LR");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.gpr[5] == 0x5678,
                "mfspr reads CTR");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.lr == 0x9abc,
                "mtspr writes LR");
    ok &= check(ppu_step(&state) == PPU_STEP_OK && state.ctr == 0xdef0,
                "mtspr writes CTR");

    uint8_t ds_code[20];
    EMIT_BE(ds_code, 0, DS_FORM(58, 5, 3, 0, 0));  /* ld */
    EMIT_BE(ds_code, 1, DS_FORM(58, 6, 3, 8, 2));  /* lwa */
    EMIT_BE(ds_code, 2, DS_FORM(62, 7, 3, 16, 0)); /* std */
    EMIT_BE(ds_code, 3, DS_FORM(58, 8, 3, 8, 1));  /* ldu */
    EMIT_BE(ds_code, 4, DS_FORM(62, 7, 3, 8, 1));  /* stdu */
    uint8_t ds_memory[32] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
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

    uint8_t halfword_code[12];
    EMIT_BE(halfword_code, 0, D_FORM(40, 7, 0, 0)); /* lhz r7,0(0) */
    EMIT_BE(halfword_code, 1, D_FORM(42, 8, 0, 0)); /* lha r8,0(0) */
    EMIT_BE(halfword_code, 2, D_FORM(44, 9, 0, 4)); /* sth r9,4(0) */

    uint8_t memory[12] = { 0, 0, 0, 0, 0x12, 0x34, 0x56, 0x78, 0, 0, 0, 0 };
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

    uint8_t update_code[28];
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
    uint8_t memory_code[24];
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
    uint8_t program_elf[140];
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
    uint8_t program_memory[24];
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
