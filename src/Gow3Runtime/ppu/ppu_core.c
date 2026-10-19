#include "ppu_core.h"

static uint32_t read_instruction_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
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
        const int bo0 = (bo & 0x10u) != 0;
        const int bo1 = (bo & 0x08u) != 0;
        const int bo2 = (bo & 0x04u) != 0;
        const int bo3 = (bo & 0x02u) != 0;
        if (!bo2)
            --state->ctr;
        const int ctr_ok = bo2 || (((state->ctr != 0) ? 1 : 0) ^ bo3);
        const int cr_bit = (int)((state->cr >> (31u - bi)) & 1u);
        const int cond_ok = bo0 || (cr_bit == bo1);
        const uint64_t next_pc = state->pc + 4;
        if ((insn & 1u) != 0)
            state->lr = next_pc;
        if (ctr_ok && cond_ok) {
            const int64_t displacement = signed_bd * 4;
            state->pc = (insn & 2u) != 0
                ? (uint64_t)displacement
                : (uint64_t)((int64_t)state->pc + displacement);
        } else {
            state->pc = next_pc;
        }
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
    case 34: /* lbz */
        {
        size_t memory_offset;
        if (!memory_range_offset(state, address, 1, &memory_offset)) return PPU_STEP_MEMORY_FAULT;
        state->gpr[rt] = state->memory[memory_offset];
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
    case 38: /* stb */
        {
        size_t memory_offset;
        if (!memory_range_offset(state, address, 1, &memory_offset)) return PPU_STEP_MEMORY_FAULT;
        state->memory[memory_offset] = (uint8_t)state->gpr[rt];
        break;
        }
    default:
        return PPU_STEP_UNSUPPORTED;
    }

    state->pc += 4;
    return PPU_STEP_OK;
}
