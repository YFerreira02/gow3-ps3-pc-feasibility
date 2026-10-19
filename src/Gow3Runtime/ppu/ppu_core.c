#include "ppu_core.h"

static uint32_t read_instruction_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint32_t rotate_left_word(uint32_t value, uint32_t shift)
{
    shift &= 31u;
    return shift == 0 ? value : (value << shift) | (value >> (32u - shift));
}

static uint32_t rotate_mask(uint32_t begin, uint32_t end)
{
    uint32_t mask = 0;
    for (uint32_t bit = 0; bit < 32; ++bit) {
        const uint32_t ppc_bit = 31u - bit;
        const int selected = begin <= end
            ? (ppc_bit >= begin && ppc_bit <= end)
            : (ppc_bit >= begin || ppc_bit <= end);
        if (selected)
            mask |= 1u << bit;
    }
    return mask;
}

static uint64_t double_bits(double value)
{
    union { double d; uint64_t u; } bits;
    bits.d = value;
    return bits.u;
}

static double double_from_bits(uint64_t value)
{
    union { double d; uint64_t u; } bits;
    bits.u = value;
    return bits.d;
}

static uint32_t float_bits(float value)
{
    union { float f; uint32_t u; } bits;
    bits.f = value;
    return bits.u;
}

static float float_from_bits(uint32_t value)
{
    union { float f; uint32_t u; } bits;
    bits.u = value;
    return bits.f;
}

void ppu_state_init(PpuState *state, const uint8_t *code, size_t code_size, uint64_t entry_pc)
{
    if (!state)
        return;
    for (size_t i = 0; i < 32; ++i)
        state->gpr[i] = 0;
    volatile double *fpr = state->fpr;
    for (size_t i = 0; i < 32; ++i)
        fpr[i] = 0.0;
    state->lr = 0;
    state->ctr = 0;
    state->cr = 0;
    state->xer_so = 0;
    state->xer_ca = 0;
    state->code = code;
    state->code_size = code_size;
    state->code_base = 0;
    state->memory = (uint8_t *)code;
    state->memory_size = code_size;
    state->memory_base = 0;
    state->pc = entry_pc;
}

void ppu_set_code_base(PpuState *state, uint64_t code_base)
{
    if (state)
        state->code_base = code_base;
}

void ppu_set_memory(PpuState *state, uint8_t *memory, size_t memory_size)
{
    ppu_set_memory_at(state, memory, memory_size, 0);
}

void ppu_set_memory_at(PpuState *state, uint8_t *memory, size_t memory_size, uint64_t guest_base)
{
    if (!state)
        return;
    state->memory = memory;
    state->memory_size = memory_size;
    state->memory_base = guest_base;
}

static int memory_range_offset(const PpuState *state, uint64_t address, size_t length, size_t *offset)
{
    if (!state->memory || address < state->memory_base)
        return 0;
    const uint64_t relative = address - state->memory_base;
    if (relative > state->memory_size || length > state->memory_size - (size_t)relative)
        return 0;
    *offset = (size_t)relative;
    return 1;
}

static int add_signed_guest_offset(uint64_t base, int64_t displacement, uint64_t *address)
{
    if (displacement >= 0) {
        const uint64_t delta = (uint64_t)displacement;
        if (base > UINT64_MAX - delta)
            return 0;
        *address = base + delta;
    } else {
        const uint64_t delta = (uint64_t)(-(displacement + 1)) + 1u;
        if (base < delta)
            return 0;
        *address = base - delta;
    }
    return 1;
}

static void set_cr_field(PpuState *state, uint32_t field_index, uint32_t relation)
{
    const uint32_t shift = 28u - (field_index * 4u);
    const uint32_t value = (relation << 1) | (state->xer_so & 1u);
    state->cr = (state->cr & ~(0x0fu << shift)) | (value << shift);
}

static void set_fp_cr_field(PpuState *state, uint32_t field_index, uint32_t fpcc)
{
    const uint32_t shift = 28u - (field_index * 4u);
    state->cr = (state->cr & ~(0x0fu << shift)) | ((fpcc & 0x0fu) << shift);
}

static void set_cr0_from_result(PpuState *state, uint64_t result)
{
    const int64_t signed_result = (int64_t)result;
    const uint32_t relation = signed_result < 0 ? 4u : (result == 0 ? 1u : 2u);
    set_cr_field(state, 0, relation);
}

static int branch_condition(PpuState *state, uint32_t bo, uint32_t bi, int decrement_ctr)
{
    const int bo0 = (bo & 0x10u) != 0;
    const int bo1 = (bo & 0x08u) != 0;
    const int bo2 = (bo & 0x04u) != 0;
    const int bo3 = (bo & 0x02u) != 0;
    if (decrement_ctr && !bo2)
        --state->ctr;
    const int ctr_ok = bo2 || (((state->ctr != 0) ? 1 : 0) ^ bo3);
    const int cr_bit = (int)((state->cr >> (31u - bi)) & 1u);
    const int cond_ok = bo0 || (cr_bit == bo1);
    return ctr_ok && cond_ok;
}

PpuStepResult ppu_step(PpuState *state)
{
    if (!state || !state->code || (state->pc & 3u) != 0 || state->pc < state->code_base ||
        state->pc - state->code_base > state->code_size ||
        state->code_size - (size_t)(state->pc - state->code_base) < 4)
        return PPU_STEP_BAD_ARGUMENT;

    const size_t code_offset = (size_t)(state->pc - state->code_base);
    const uint32_t insn = read_instruction_be(state->code + code_offset);
    const uint32_t opcode = insn >> 26;
    const uint32_t rt = (insn >> 21) & 31u;
    const uint32_t ra = (insn >> 16) & 31u;
    const uint16_t imm = (uint16_t)(insn & 0xffffu);
    const int64_t signed_imm = (int16_t)imm;
    const uint64_t base = ra == 0 ? 0 : state->gpr[ra];
    uint64_t address = 0;
    const int address_valid = add_signed_guest_offset(base, signed_imm, &address);

    if (opcode == 17u) { /* sc: hand off to the future PS3 service dispatcher */
        if (insn != ((17u << 26) | 2u))
            return PPU_STEP_UNSUPPORTED;
        state->pc += 4;
        return PPU_STEP_SYSCALL;
    }

    switch (opcode) {
    case 10: /* cmpli */
    case 11: { /* cmpi */
        const uint32_t bf = (insn >> 23) & 7u;
        const int is_64 = (insn & (1u << 21)) != 0;
        const uint64_t value = state->gpr[ra];
        uint32_t relation;
        if (opcode == 11) {
            const int64_t left = is_64 ? (int64_t)value : (int64_t)(int32_t)(uint32_t)value;
            if (left < signed_imm) relation = 4;
            else if (left > signed_imm) relation = 2;
            else relation = 1;
        } else {
            const uint64_t left = is_64 ? value : (uint32_t)value;
            if (left < imm) relation = 4;
            else if (left > imm) relation = 2;
            else relation = 1;
        }
        set_cr_field(state, bf, relation);
        break;
    }
    case 7: /* mulli: modulo-64-bit product by signed immediate */
        state->gpr[rt] = state->gpr[ra] * (uint64_t)signed_imm;
        break;
    case 14: /* addi */
        state->gpr[rt] = base + (uint64_t)signed_imm;
        break;
    case 15: /* addis */
        state->gpr[rt] = base + ((uint64_t)signed_imm << 16);
        break;
    case 24: { /* ori: RS is in the RT field; RA is destination */
        const uint32_t rs = rt;
        state->gpr[ra] = state->gpr[rs] | (uint64_t)imm;
        break;
    }
    case 25: /* oris */
        state->gpr[ra] = state->gpr[rt] | ((uint64_t)imm << 16);
        break;
    case 26: /* xori */
        state->gpr[ra] = state->gpr[rt] ^ (uint64_t)imm;
        break;
    case 27: /* xoris */
        state->gpr[ra] = state->gpr[rt] ^ ((uint64_t)imm << 16);
        break;
    case 28: /* andi. */
        state->gpr[ra] = state->gpr[rt] & (uint64_t)imm;
        set_cr_field(state, 0, state->gpr[ra] == 0 ? 1u : 2u);
        break;
    case 29: /* andis. */
        state->gpr[ra] = state->gpr[rt] & ((uint64_t)imm << 16);
        set_cr_field(state, 0, state->gpr[ra] == 0 ? 1u : 2u);
        break;
    case 18: { /* b: unconditional branch */
        const uint32_t li = (insn >> 2) & 0x00ffffffu;
        const int64_t signed_li = (li & 0x00800000u)
            ? (int64_t)li - 0x01000000ll
            : (int64_t)li;
        const uint64_t next_pc = state->pc + 4;
        const int absolute = (insn & 2u) != 0;
        const int link = (insn & 1u) != 0;
        const int64_t displacement = signed_li * 4;
        if (link)
            state->lr = next_pc;
        state->pc = absolute ? (uint64_t)displacement
                             : (uint64_t)((int64_t)state->pc + displacement);
        return PPU_STEP_OK;
    }
    case 16: { /* bc: conditional branch */
        const uint32_t bo = (insn >> 21) & 31u;
        const uint32_t bi = (insn >> 16) & 31u;
        const uint32_t bd = (insn >> 2) & 0x3fffu;
        const int64_t signed_bd = (bd & 0x2000u)
            ? (int64_t)bd - 0x4000ll
            : (int64_t)bd;
        const uint64_t next_pc = state->pc + 4;
        if ((insn & 1u) != 0)
            state->lr = next_pc;
        if (branch_condition(state, bo, bi, 1)) {
            const int64_t displacement = signed_bd * 4;
            state->pc = (insn & 2u) != 0
                ? (uint64_t)displacement
                : (uint64_t)((int64_t)state->pc + displacement);
        } else {
            state->pc = next_pc;
        }
        return PPU_STEP_OK;
    }
    case 19: { /* XL-form indirect branches */
        const uint32_t bo = (insn >> 21) & 31u;
        const uint32_t bi = (insn >> 16) & 31u;
        const uint32_t xo = (insn >> 1) & 0x3ffu;
        const uint64_t next_pc = state->pc + 4;
        if (xo == 33u || xo == 129u || xo == 193u || xo == 225u ||
            xo == 257u || xo == 289u || xo == 417u || xo == 449u) {
            const uint32_t bt = rt;
            const uint32_t ba = ra;
            const uint32_t bb = (insn >> 11) & 31u;
            const uint32_t a = (state->cr >> (31u - ba)) & 1u;
            const uint32_t b = (state->cr >> (31u - bb)) & 1u;
            uint32_t value;
            if ((insn & 1u) != 0)
                return PPU_STEP_UNSUPPORTED;
            switch (xo) {
            case 33: value = !(a | b); break;        /* crnor */
            case 129: value = a & !b; break;         /* crandc */
            case 193: value = a ^ b; break;          /* crxor */
            case 225: value = !(a & b); break;       /* crnand */
            case 257: value = a & b; break;          /* crand */
            case 289: value = !(a ^ b); break;       /* creqv */
            case 417: value = a | !b; break;         /* crorc */
            default: value = a | b; break;           /* cror */
            }
            const uint32_t target_mask = 1u << (31u - bt);
            state->cr = value ? (state->cr | target_mask) : (state->cr & ~target_mask);
            state->pc = next_pc;
            return PPU_STEP_OK;
        }
        if (xo == 16u) { /* bclr */
            const uint64_t target = state->lr & ~3ull;
            if ((insn & 1u) != 0)
                state->lr = next_pc;
            if (branch_condition(state, bo, bi, 1))
                state->pc = target;
            else
                state->pc = next_pc;
            return PPU_STEP_OK;
        }
        if (xo == 528u) { /* bcctr: CTR cannot be decremented by this form */
            if ((bo & 4u) == 0)
                return PPU_STEP_UNSUPPORTED;
            const uint64_t target = state->ctr & ~3ull;
            const int bo0 = (bo & 0x10u) != 0;
            const int bo1 = (bo & 0x08u) != 0;
            const int cr_bit = (int)((state->cr >> (31u - bi)) & 1u);
            const int cond_ok = bo0 || (cr_bit == bo1);
            if ((insn & 1u) != 0)
                state->lr = next_pc;
            state->pc = cond_ok ? target : next_pc;
            return PPU_STEP_OK;
        }
        return PPU_STEP_UNSUPPORTED;
    }
    case 20: /* rlwimi */
    case 21: /* rlwinm */
    case 23: { /* rlwnm */
        const uint32_t rs = rt;
        const uint32_t shift_field = (insn >> 11) & 31u;
        const uint32_t shift = opcode == 23 ? (uint32_t)state->gpr[shift_field] : shift_field;
        const uint32_t mb = (insn >> 6) & 31u;
        const uint32_t me = (insn >> 1) & 31u;
        const uint32_t mask = rotate_mask(mb, me);
        const uint32_t rotated = rotate_left_word((uint32_t)state->gpr[rs], shift);
        const uint32_t result = opcode == 20
            ? (((uint32_t)state->gpr[ra] & ~mask) | (rotated & mask))
            : (rotated & mask);
        state->gpr[ra] = result;
        if ((insn & 1u) != 0)
            set_cr0_from_result(state, result);
        state->pc += 4;
        return PPU_STEP_OK;
    }
    case 63: { /* selected double-precision floating-point operations */
        const uint32_t xo5 = (insn >> 1) & 31u;
        const uint32_t xo10 = (insn >> 1) & 0x3ffu;
        const uint32_t frb = (insn >> 11) & 31u;
        if ((insn & 1u) != 0)
            return PPU_STEP_UNSUPPORTED;
        if (xo10 == 0u || xo10 == 32u) { /* fcmpu / fcmpo */
            const double left = state->fpr[ra];
            const double right = state->fpr[frb];
            const uint32_t field = (insn >> 23) & 7u;
            if (left != left || right != right) {
                if (xo10 == 32u)
                    return PPU_STEP_UNSUPPORTED; /* fcmpo raises an FP exception on NaN */
                set_fp_cr_field(state, field, 1u); /* unordered */
            } else {
                const uint32_t fpcc = left < right ? 8u : (left > right ? 4u : 2u);
                set_fp_cr_field(state, field, fpcc);
            }
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo10 == 12u) { /* frsp: round to single precision, retain FPR format */
            state->fpr[rt] = (double)(float)state->fpr[frb];
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo5 == 23u) { /* fsel: choose FRC for nonnegative FRA, otherwise FRB */
            const uint32_t frc = (insn >> 6) & 31u;
            state->fpr[rt] = state->fpr[ra] >= 0.0 ? state->fpr[frc] : state->fpr[frb];
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo5 == 21u) state->fpr[rt] = state->fpr[ra] + state->fpr[frb]; /* fadd */
        else if (xo5 == 20u) state->fpr[rt] = state->fpr[ra] - state->fpr[frb]; /* fsub */
        else if (xo5 == 25u) state->fpr[rt] = state->fpr[ra] * state->fpr[(insn >> 6) & 31u]; /* fmul */
        else if (xo5 == 18u) {
            const double divisor = state->fpr[frb];
            if (divisor == 0.0) return PPU_STEP_UNSUPPORTED;
            state->fpr[rt] = state->fpr[ra] / divisor; /* fdiv */
        } else if (xo10 == 72u) state->fpr[rt] = state->fpr[frb]; /* fmr */
        else if (xo10 == 264u) state->fpr[rt] = double_from_bits(double_bits(state->fpr[frb]) & 0x7fffffffffffffffull); /* fabs */
        else if (xo10 == 136u) state->fpr[rt] = double_from_bits(double_bits(state->fpr[frb]) | 0x8000000000000000ull); /* fnabs */
        else if (xo10 == 40u) state->fpr[rt] = double_from_bits(double_bits(state->fpr[frb]) ^ 0x8000000000000000ull); /* fneg */
        else return PPU_STEP_UNSUPPORTED;
        state->pc += 4;
        return PPU_STEP_OK;
    }
    case 31: { /* selected common X-form integer instructions */
        const uint32_t rb = (insn >> 11) & 31u;
        const uint32_t xo = (insn >> 1) & 0x3ffu;
        const int record = (insn & 1u) != 0;
        uint64_t result;
        {
            size_t width = 0;
            int is_load = 0, update = 0, sign_extend = 0;
            switch (xo) {
            case 23: width = 4; is_load = 1; break;  /* lwzx */
            case 55: width = 4; is_load = 1; update = 1; break; /* lwzux */
            case 87: width = 1; is_load = 1; break;  /* lbzx */
            case 119: width = 1; is_load = 1; update = 1; break; /* lbzux */
            case 279: width = 2; is_load = 1; break; /* lhzx */
            case 311: width = 2; is_load = 1; update = 1; break; /* lhzux */
            case 343: width = 2; is_load = 1; sign_extend = 1; break; /* lhax */
            case 375: width = 2; is_load = 1; update = 1; sign_extend = 1; break; /* lhaux */
            case 21: width = 8; is_load = 1; break;  /* ldx */
            case 53: width = 8; is_load = 1; update = 1; break; /* ldux */
            case 341: width = 4; is_load = 1; sign_extend = 1; break; /* lwax */
            case 151: width = 4; break;             /* stwx */
            case 183: width = 4; update = 1; break; /* stwux */
            case 215: width = 1; break;             /* stbx */
            case 247: width = 1; update = 1; break; /* stbux */
            case 407: width = 2; break;             /* sthx */
            case 439: width = 2; update = 1; break; /* sthux */
            case 149: width = 8; break;             /* stdx */
            case 181: width = 8; update = 1; break; /* stdux */
            default: break;
            }
            if (width != 0) {
                const uint64_t indexed_base = ra == 0 ? 0 : state->gpr[ra];
                if (indexed_base > UINT64_MAX - state->gpr[rb])
                    return PPU_STEP_MEMORY_FAULT;
                const uint64_t indexed_address = indexed_base + state->gpr[rb];
                size_t memory_offset;
                if (record || (update && (ra == 0 || (is_load && rt == ra))))
                    return PPU_STEP_UNSUPPORTED;
                if ((indexed_address & (width - 1u)) != 0 ||
                    !memory_range_offset(state, indexed_address, width, &memory_offset))
                    return PPU_STEP_MEMORY_FAULT;
                if (is_load) {
                    uint64_t loaded = 0;
                    for (size_t i = 0; i < width; ++i)
                        loaded = (loaded << 8) | state->memory[memory_offset + i];
                    if (sign_extend && width == 2)
                        loaded = (uint64_t)(int64_t)(int16_t)loaded;
                    else if (sign_extend && width == 4)
                        loaded = (uint64_t)(int64_t)(int32_t)loaded;
                    state->gpr[rt] = loaded;
                } else {
                    for (size_t i = 0; i < width; ++i)
                        state->memory[memory_offset + i] =
                            (uint8_t)(state->gpr[rt] >> ((uint32_t)(width - 1u - i) * 8u));
                }
                if (update)
                    state->gpr[ra] = indexed_address;
                state->pc += 4;
                return PPU_STEP_OK;
            }
        }
        if (xo == 0u || xo == 32u) { /* cmp / cmpl */
            const uint32_t bf = (insn >> 23) & 7u;
            const int is_64 = (insn & (1u << 21)) != 0;
            const uint64_t left = state->gpr[ra];
            const uint64_t right = state->gpr[rb];
            uint32_t relation;
            if (xo == 0u) {
                const int64_t signed_left = is_64 ? (int64_t)left : (int64_t)(int32_t)(uint32_t)left;
                const int64_t signed_right = is_64 ? (int64_t)right : (int64_t)(int32_t)(uint32_t)right;
                relation = signed_left < signed_right ? 4u : (signed_left > signed_right ? 2u : 1u);
            } else {
                const uint64_t unsigned_left = is_64 ? left : (uint32_t)left;
                const uint64_t unsigned_right = is_64 ? right : (uint32_t)right;
                relation = unsigned_left < unsigned_right ? 4u : (unsigned_left > unsigned_right ? 2u : 1u);
            }
            set_cr_field(state, bf, relation);
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo == 339u || xo == 467u) { /* mfspr / mtspr, limited to LR and CTR */
            const uint32_t spr = ((insn >> 16) & 31u) | (((insn >> 11) & 31u) << 5);
            if (record || (spr != 8u && spr != 9u))
                return PPU_STEP_UNSUPPORTED;
            if (xo == 339u)
                state->gpr[rt] = spr == 8u ? state->lr : state->ctr;
            else if (spr == 8u)
                state->lr = state->gpr[rt];
            else
                state->ctr = state->gpr[rt];
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo == 19u) { /* mfcr */
            if (record)
                return PPU_STEP_UNSUPPORTED;
            state->gpr[rt] = state->cr;
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo == 144u) { /* mtcrf */
            const uint32_t field_mask = (insn >> 12) & 0xffu;
            if (record || field_mask == 0)
                return PPU_STEP_UNSUPPORTED;
            for (uint32_t field = 0; field < 8; ++field) {
                if ((field_mask & (1u << (7u - field))) != 0) {
                    const uint32_t shift = 28u - (field * 4u);
                    const uint32_t mask = 0x0fu << shift;
                    state->cr = (state->cr & ~mask) | ((uint32_t)state->gpr[rt] & mask);
                }
            }
            state->pc += 4;
            return PPU_STEP_OK;
        }
        switch (xo) {
        case 266: /* add */
            if ((insn & (1u << 10)) != 0) return PPU_STEP_UNSUPPORTED; /* OE */
            result = state->gpr[ra] + state->gpr[rb];
            break;
        case 10: /* addc */
        case 138: { /* adde */
            if ((insn & (1u << 10)) != 0) return PPU_STEP_UNSUPPORTED;
            const uint64_t carry_in = xo == 138u ? (state->xer_ca & 1u) : 0u;
            const uint64_t first = state->gpr[ra] + state->gpr[rb];
            const uint8_t carry_first = first < state->gpr[ra];
            result = first + carry_in;
            state->xer_ca = (uint8_t)(carry_first || result < first);
            break;
        }
        case 40: /* subf: RB - RA */
            if ((insn & (1u << 10)) != 0) return PPU_STEP_UNSUPPORTED; /* OE */
            result = state->gpr[rb] - state->gpr[ra];
            break;
        case 8: /* subfc: RB - RA, CA means no borrow */
            if ((insn & (1u << 10)) != 0) return PPU_STEP_UNSUPPORTED;
            result = state->gpr[rb] - state->gpr[ra];
            state->xer_ca = (uint8_t)(state->gpr[rb] >= state->gpr[ra]);
            break;
        case 136: { /* subfe: RB - RA - (1 - CA) */
            if ((insn & (1u << 10)) != 0) return PPU_STEP_UNSUPPORTED;
            const uint64_t borrow_in = (state->xer_ca & 1u) ? 0u : 1u;
            const uint64_t first = state->gpr[rb] - state->gpr[ra];
            const uint8_t borrow_first = state->gpr[rb] < state->gpr[ra];
            result = first - borrow_in;
            const uint8_t borrow_second = borrow_in && first == 0;
            state->xer_ca = (uint8_t)(!borrow_first && !borrow_second);
            break;
        }
        case 202: { /* addze */
            if ((insn & (1u << 10)) != 0) return PPU_STEP_UNSUPPORTED;
            const uint64_t carry_in = state->xer_ca & 1u;
            result = state->gpr[ra] + carry_in;
            state->xer_ca = (uint8_t)(carry_in && result == 0);
            break;
        }
        case 28: result = state->gpr[ra] & state->gpr[rb]; break; /* and */
        case 60: result = state->gpr[ra] & ~state->gpr[rb]; break; /* andc */
        case 124: result = ~(state->gpr[ra] | state->gpr[rb]); break; /* nor */
        case 284: result = ~(state->gpr[ra] ^ state->gpr[rb]); break; /* eqv */
        case 412: result = state->gpr[ra] | ~state->gpr[rb]; break; /* orc */
        case 476: result = ~(state->gpr[ra] & state->gpr[rb]); break; /* nand */
        case 104: result = 0u - state->gpr[ra]; break; /* neg */
        case 24: { /* slw */
            const uint64_t shift = state->gpr[rb];
            result = (shift & ~31ull) != 0 ? 0 : (uint32_t)state->gpr[ra] << (shift & 31u);
            break;
        }
        case 536: { /* srw */
            const uint64_t shift = state->gpr[rb];
            result = (shift & ~31ull) != 0 ? 0 : (uint32_t)state->gpr[ra] >> (shift & 31u);
            break;
        }
        case 26: { /* cntlzw */
            uint32_t value = (uint32_t)state->gpr[ra];
            uint32_t count = 0;
            while (count < 32u && (value & 0x80000000u) == 0) {
                ++count;
                value <<= 1;
            }
            result = count;
            break;
        }
        case 954: result = (uint64_t)(int64_t)(int8_t)state->gpr[ra]; break; /* extsb */
        case 922: result = (uint64_t)(int64_t)(int16_t)state->gpr[ra]; break; /* extsh */
        case 986: result = (uint64_t)(int64_t)(int32_t)state->gpr[ra]; break; /* extsw */
        case 233: result = state->gpr[ra] * state->gpr[rb]; break; /* mulld */
        case 235: result = (uint32_t)state->gpr[ra] * (uint32_t)state->gpr[rb]; break; /* mullw */
        case 491: { /* divw */
            const int32_t divisor = (int32_t)state->gpr[rb];
            if (divisor == 0) return PPU_STEP_UNSUPPORTED;
            result = (uint64_t)(int64_t)((int32_t)state->gpr[ra] / divisor);
            break;
        }
        case 459: { /* divwu */
            const uint32_t divisor = (uint32_t)state->gpr[rb];
            if (divisor == 0) return PPU_STEP_UNSUPPORTED;
            result = (uint32_t)state->gpr[ra] / divisor;
            break;
        }
        case 489: { /* divd */
            const int64_t divisor = (int64_t)state->gpr[rb];
            const int64_t dividend = (int64_t)state->gpr[ra];
            if (divisor == 0 || (dividend == INT64_MIN && divisor == -1))
                return PPU_STEP_UNSUPPORTED;
            result = (uint64_t)(dividend / divisor);
            break;
        }
        case 457: { /* divdu */
            if (state->gpr[rb] == 0)
                return PPU_STEP_UNSUPPORTED;
            result = state->gpr[ra] / state->gpr[rb];
            break;
        }
        case 316: result = state->gpr[ra] ^ state->gpr[rb]; break; /* xor */
        case 444: result = state->gpr[ra] | state->gpr[rb]; break; /* or */
        default: return PPU_STEP_UNSUPPORTED;
        }
        state->gpr[rt] = result;
        if (record)
            set_cr0_from_result(state, result);
        state->pc += 4;
        return PPU_STEP_OK;
    }
    case 32: /* lwz */
        {
        size_t memory_offset;
        if (!address_valid || (address & 3u) != 0 || !memory_range_offset(state, address, 4, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        state->gpr[rt] = ((uint32_t)state->memory[memory_offset] << 24) |
                         ((uint32_t)state->memory[memory_offset + 1] << 16) |
                         ((uint32_t)state->memory[memory_offset + 2] << 8) |
                         (uint32_t)state->memory[memory_offset + 3];
        break;
        }
    case 33: /* lwzu */
        {
        size_t memory_offset;
        if (ra == 0 || rt == ra) return PPU_STEP_UNSUPPORTED;
        if (!address_valid || (address & 3u) != 0 || !memory_range_offset(state, address, 4, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        state->gpr[rt] = ((uint32_t)state->memory[memory_offset] << 24) |
                         ((uint32_t)state->memory[memory_offset + 1] << 16) |
                         ((uint32_t)state->memory[memory_offset + 2] << 8) |
                         (uint32_t)state->memory[memory_offset + 3];
        state->gpr[ra] = address;
        break;
        }
    case 34: /* lbz */
        {
        size_t memory_offset;
        if (!address_valid || !memory_range_offset(state, address, 1, &memory_offset)) return PPU_STEP_MEMORY_FAULT;
        state->gpr[rt] = state->memory[memory_offset];
        break;
        }
    case 35: /* lbzu */
        {
        size_t memory_offset;
        if (ra == 0 || rt == ra) return PPU_STEP_UNSUPPORTED;
        if (!address_valid || !memory_range_offset(state, address, 1, &memory_offset)) return PPU_STEP_MEMORY_FAULT;
        state->gpr[rt] = state->memory[memory_offset];
        state->gpr[ra] = address;
        break;
        }
    case 36: /* stw */
        {
        size_t memory_offset;
        if (!address_valid || (address & 3u) != 0 || !memory_range_offset(state, address, 4, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        state->memory[memory_offset] = (uint8_t)(state->gpr[rt] >> 24);
        state->memory[memory_offset + 1] = (uint8_t)(state->gpr[rt] >> 16);
        state->memory[memory_offset + 2] = (uint8_t)(state->gpr[rt] >> 8);
        state->memory[memory_offset + 3] = (uint8_t)state->gpr[rt];
        break;
        }
    case 37: /* stwu */
        {
        size_t memory_offset;
        if (ra == 0) return PPU_STEP_UNSUPPORTED;
        if (!address_valid || (address & 3u) != 0 || !memory_range_offset(state, address, 4, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        state->memory[memory_offset] = (uint8_t)(state->gpr[rt] >> 24);
        state->memory[memory_offset + 1] = (uint8_t)(state->gpr[rt] >> 16);
        state->memory[memory_offset + 2] = (uint8_t)(state->gpr[rt] >> 8);
        state->memory[memory_offset + 3] = (uint8_t)state->gpr[rt];
        state->gpr[ra] = address;
        break;
        }
    case 40: /* lhz */
        {
        size_t memory_offset;
        if (!address_valid || (address & 1u) != 0 || !memory_range_offset(state, address, 2, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        state->gpr[rt] = ((uint16_t)state->memory[memory_offset] << 8) |
                         (uint16_t)state->memory[memory_offset + 1];
        break;
        }
    case 41: /* lhzu */
        {
        size_t memory_offset;
        if (ra == 0 || rt == ra) return PPU_STEP_UNSUPPORTED;
        if (!address_valid || (address & 1u) != 0 || !memory_range_offset(state, address, 2, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        state->gpr[rt] = ((uint16_t)state->memory[memory_offset] << 8) |
                         (uint16_t)state->memory[memory_offset + 1];
        state->gpr[ra] = address;
        break;
        }
    case 42: /* lha */
        {
        size_t memory_offset;
        if (!address_valid || (address & 1u) != 0 || !memory_range_offset(state, address, 2, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        const uint16_t value = ((uint16_t)state->memory[memory_offset] << 8) |
                               (uint16_t)state->memory[memory_offset + 1];
        state->gpr[rt] = (uint64_t)(int64_t)(int16_t)value;
        break;
        }
    case 43: /* lhau */
        {
        size_t memory_offset;
        if (ra == 0 || rt == ra) return PPU_STEP_UNSUPPORTED;
        if (!address_valid || (address & 1u) != 0 || !memory_range_offset(state, address, 2, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        const uint16_t value = ((uint16_t)state->memory[memory_offset] << 8) |
                               (uint16_t)state->memory[memory_offset + 1];
        state->gpr[rt] = (uint64_t)(int64_t)(int16_t)value;
        state->gpr[ra] = address;
        break;
        }
    case 44: /* sth */
        {
        size_t memory_offset;
        if (!address_valid || (address & 1u) != 0 || !memory_range_offset(state, address, 2, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        state->memory[memory_offset] = (uint8_t)(state->gpr[rt] >> 8);
        state->memory[memory_offset + 1] = (uint8_t)state->gpr[rt];
        break;
        }
    case 45: /* sthu */
        {
        size_t memory_offset;
        if (ra == 0) return PPU_STEP_UNSUPPORTED;
        if (!address_valid || (address & 1u) != 0 || !memory_range_offset(state, address, 2, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        state->memory[memory_offset] = (uint8_t)(state->gpr[rt] >> 8);
        state->memory[memory_offset + 1] = (uint8_t)state->gpr[rt];
        state->gpr[ra] = address;
        break;
        }
    case 48: case 49: case 50: case 51: /* lfs[u], lfd[u] */
    case 52: case 53: case 54: case 55: { /* stfs[u], stfd[u] */
        const uint32_t form = opcode & 3u;
        const int is_single = form == 0u || form == 1u;
        const int update = (opcode & 1u) != 0;
        const int is_store = opcode >= 52u;
        const size_t width = is_single ? 4u : 8u;
        size_t memory_offset;
        if (update && ra == 0)
            return PPU_STEP_UNSUPPORTED;
        if (!address_valid || (address & (width - 1u)) != 0 ||
            !memory_range_offset(state, address, width, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        if (is_store) {
            const uint64_t value = is_single
                ? float_bits((float)state->fpr[rt])
                : double_bits(state->fpr[rt]);
            for (size_t i = 0; i < width; ++i)
                state->memory[memory_offset + i] =
                    (uint8_t)(value >> ((uint32_t)(width - 1u - i) * 8u));
        } else {
            uint64_t value = 0;
            for (size_t i = 0; i < width; ++i)
                value = (value << 8) | state->memory[memory_offset + i];
            state->fpr[rt] = is_single
                ? (double)float_from_bits((uint32_t)value)
                : double_from_bits(value);
        }
        if (update)
            state->gpr[ra] = address;
        break;
    }
    case 58: { /* DS-form ld, ldu, lwa */
        const uint32_t xo = insn & 3u;
        const int64_t displacement = (int16_t)(insn & 0xfffcu);
        const uint64_t ds_base = ra == 0 ? 0 : state->gpr[ra];
        uint64_t ds_address;
        const int ds_address_valid = add_signed_guest_offset(ds_base, displacement, &ds_address);
        size_t memory_offset;
        const size_t width = xo == 2u ? 4u : 8u;
        if (xo == 3u || (xo == 1u && ra == 0) || (xo == 1u && rt == ra))
            return PPU_STEP_UNSUPPORTED;
        if (!ds_address_valid || (ds_address & (width - 1u)) != 0 ||
            !memory_range_offset(state, ds_address, width, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        if (xo == 2u) {
            const uint32_t value = ((uint32_t)state->memory[memory_offset] << 24) |
                ((uint32_t)state->memory[memory_offset + 1] << 16) |
                ((uint32_t)state->memory[memory_offset + 2] << 8) |
                (uint32_t)state->memory[memory_offset + 3];
            state->gpr[rt] = (uint64_t)(int64_t)(int32_t)value;
        } else {
            uint64_t value = 0;
            for (size_t i = 0; i < 8; ++i)
                value = (value << 8) | state->memory[memory_offset + i];
            state->gpr[rt] = value;
        }
        if (xo == 1u)
            state->gpr[ra] = ds_address;
        break;
    }
    case 59: { /* selected single-precision floating-point arithmetic */
        const uint32_t xo5 = (insn >> 1) & 31u;
        const uint32_t fra = ra;
        const uint32_t frb = (insn >> 11) & 31u;
        const uint32_t frc = (insn >> 6) & 31u;
        if ((insn & 1u) != 0)
            return PPU_STEP_UNSUPPORTED;
        const float left = (float)state->fpr[fra];
        const float right = (float)state->fpr[frb];
        float value;
        if (xo5 == 21u) value = left + right; /* fadds */
        else if (xo5 == 20u) value = left - right; /* fsubs */
        else if (xo5 == 25u) value = left * (float)state->fpr[frc]; /* fmuls */
        else if (xo5 == 18u) {
            if (right == 0.0f) return PPU_STEP_UNSUPPORTED;
            value = left / right; /* fdivs */
        } else return PPU_STEP_UNSUPPORTED;
        state->fpr[rt] = (double)value;
        state->pc += 4;
        return PPU_STEP_OK;
    }
    case 62: { /* DS-form std, stdu */
        const uint32_t xo = insn & 3u;
        const int64_t displacement = (int16_t)(insn & 0xfffcu);
        const uint64_t ds_base = ra == 0 ? 0 : state->gpr[ra];
        uint64_t ds_address;
        const int ds_address_valid = add_signed_guest_offset(ds_base, displacement, &ds_address);
        size_t memory_offset;
        if (xo > 1u || (xo == 1u && ra == 0))
            return PPU_STEP_UNSUPPORTED;
        if (!ds_address_valid || (ds_address & 7u) != 0 || !memory_range_offset(state, ds_address, 8, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        for (size_t i = 0; i < 8; ++i)
            state->memory[memory_offset + i] = (uint8_t)(state->gpr[rt] >> (56u - (uint32_t)(i * 8u)));
        if (xo == 1u)
            state->gpr[ra] = ds_address;
        break;
    }
    case 38: /* stb */
        {
        size_t memory_offset;
        if (!address_valid || !memory_range_offset(state, address, 1, &memory_offset)) return PPU_STEP_MEMORY_FAULT;
        state->memory[memory_offset] = (uint8_t)state->gpr[rt];
        break;
        }
    case 39: /* stbu */
        {
        size_t memory_offset;
        if (ra == 0) return PPU_STEP_UNSUPPORTED;
        if (!address_valid || !memory_range_offset(state, address, 1, &memory_offset)) return PPU_STEP_MEMORY_FAULT;
        state->memory[memory_offset] = (uint8_t)state->gpr[rt];
        state->gpr[ra] = address;
        break;
        }
    default:
        return PPU_STEP_UNSUPPORTED;
    }

    state->pc += 4;
    return PPU_STEP_OK;
}

PpuStepResult ppu_run(PpuState *state, uint64_t instruction_budget, uint64_t *executed)
{
    uint64_t count = 0;
    if (executed)
        *executed = 0;
    if (!state || instruction_budget == 0)
        return PPU_STEP_BAD_ARGUMENT;
    while (count < instruction_budget) {
        const PpuStepResult result = ppu_step(state);
        if (result != PPU_STEP_OK && result != PPU_STEP_SYSCALL) {
            if (executed) *executed = count;
            return result;
        }
        ++count;
        if (result == PPU_STEP_SYSCALL) {
            if (executed) *executed = count;
            return result;
        }
    }
    if (executed) *executed = count;
    return PPU_STEP_LIMIT_REACHED;
}
