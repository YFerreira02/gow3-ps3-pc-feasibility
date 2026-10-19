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

void ppu_state_init(PpuState *state, const uint8_t *code, size_t code_size, uint64_t entry_pc)
{
    if (!state)
        return;
    for (size_t i = 0; i < 32; ++i)
        state->gpr[i] = 0;
    state->lr = 0;
    state->ctr = 0;
    state->cr = 0;
    state->xer_so = 0;
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

static void set_cr_field(PpuState *state, uint32_t field_index, uint32_t relation)
{
    const uint32_t shift = 28u - (field_index * 4u);
    const uint32_t value = (relation << 1) | (state->xer_so & 1u);
    state->cr = (state->cr & ~(0x0fu << shift)) | (value << shift);
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
    const uint64_t address = base + (uint64_t)signed_imm;

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
    case 31: { /* selected common X-form integer instructions */
        const uint32_t rb = (insn >> 11) & 31u;
        const uint32_t xo = (insn >> 1) & 0x3ffu;
        const int record = (insn & 1u) != 0;
        uint64_t result;
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
        switch (xo) {
        case 266: /* add */
            if ((insn & (1u << 10)) != 0) return PPU_STEP_UNSUPPORTED; /* OE */
            result = state->gpr[ra] + state->gpr[rb];
            break;
        case 40: /* subf: RB - RA */
            if ((insn & (1u << 10)) != 0) return PPU_STEP_UNSUPPORTED; /* OE */
            result = state->gpr[rb] - state->gpr[ra];
            break;
        case 28: result = state->gpr[ra] & state->gpr[rb]; break; /* and */
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
        if ((address & 3u) != 0 || !memory_range_offset(state, address, 4, &memory_offset))
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
        if ((address & 3u) != 0 || !memory_range_offset(state, address, 4, &memory_offset))
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
        if (!memory_range_offset(state, address, 1, &memory_offset)) return PPU_STEP_MEMORY_FAULT;
        state->gpr[rt] = state->memory[memory_offset];
        break;
        }
    case 35: /* lbzu */
        {
        size_t memory_offset;
        if (ra == 0 || rt == ra) return PPU_STEP_UNSUPPORTED;
        if (!memory_range_offset(state, address, 1, &memory_offset)) return PPU_STEP_MEMORY_FAULT;
        state->gpr[rt] = state->memory[memory_offset];
        state->gpr[ra] = address;
        break;
        }
    case 36: /* stw */
        {
        size_t memory_offset;
        if ((address & 3u) != 0 || !memory_range_offset(state, address, 4, &memory_offset))
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
        if ((address & 3u) != 0 || !memory_range_offset(state, address, 4, &memory_offset))
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
        if ((address & 1u) != 0 || !memory_range_offset(state, address, 2, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        state->gpr[rt] = ((uint16_t)state->memory[memory_offset] << 8) |
                         (uint16_t)state->memory[memory_offset + 1];
        break;
        }
    case 41: /* lhzu */
        {
        size_t memory_offset;
        if (ra == 0 || rt == ra) return PPU_STEP_UNSUPPORTED;
        if ((address & 1u) != 0 || !memory_range_offset(state, address, 2, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        state->gpr[rt] = ((uint16_t)state->memory[memory_offset] << 8) |
                         (uint16_t)state->memory[memory_offset + 1];
        state->gpr[ra] = address;
        break;
        }
    case 42: /* lha */
        {
        size_t memory_offset;
        if ((address & 1u) != 0 || !memory_range_offset(state, address, 2, &memory_offset))
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
        if ((address & 1u) != 0 || !memory_range_offset(state, address, 2, &memory_offset))
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
        if ((address & 1u) != 0 || !memory_range_offset(state, address, 2, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        state->memory[memory_offset] = (uint8_t)(state->gpr[rt] >> 8);
        state->memory[memory_offset + 1] = (uint8_t)state->gpr[rt];
        break;
        }
    case 45: /* sthu */
        {
        size_t memory_offset;
        if (ra == 0) return PPU_STEP_UNSUPPORTED;
        if ((address & 1u) != 0 || !memory_range_offset(state, address, 2, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        state->memory[memory_offset] = (uint8_t)(state->gpr[rt] >> 8);
        state->memory[memory_offset + 1] = (uint8_t)state->gpr[rt];
        state->gpr[ra] = address;
        break;
        }
    case 58: { /* DS-form ld, ldu, lwa */
        const uint32_t xo = insn & 3u;
        const int64_t displacement = (int16_t)(insn & 0xfffcu);
        const uint64_t ds_base = ra == 0 ? 0 : state->gpr[ra];
        const uint64_t ds_address = ds_base + (uint64_t)displacement;
        size_t memory_offset;
        const size_t width = xo == 2u ? 4u : 8u;
        if (xo == 3u || (xo == 1u && ra == 0) || (xo == 1u && rt == ra))
            return PPU_STEP_UNSUPPORTED;
        if ((ds_address & (width - 1u)) != 0 ||
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
    case 62: { /* DS-form std, stdu */
        const uint32_t xo = insn & 3u;
        const int64_t displacement = (int16_t)(insn & 0xfffcu);
        const uint64_t ds_base = ra == 0 ? 0 : state->gpr[ra];
        const uint64_t ds_address = ds_base + (uint64_t)displacement;
        size_t memory_offset;
        if (xo > 1u || (xo == 1u && ra == 0))
            return PPU_STEP_UNSUPPORTED;
        if ((ds_address & 7u) != 0 || !memory_range_offset(state, ds_address, 8, &memory_offset))
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
        if (!memory_range_offset(state, address, 1, &memory_offset)) return PPU_STEP_MEMORY_FAULT;
        state->memory[memory_offset] = (uint8_t)state->gpr[rt];
        break;
        }
    case 39: /* stbu */
        {
        size_t memory_offset;
        if (ra == 0) return PPU_STEP_UNSUPPORTED;
        if (!memory_range_offset(state, address, 1, &memory_offset)) return PPU_STEP_MEMORY_FAULT;
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
