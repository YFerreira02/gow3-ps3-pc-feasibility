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
    state->code = code;
    state->code_size = code_size;
    state->memory = (uint8_t *)code;
    state->memory_size = code_size;
    state->pc = entry_pc;
}

void ppu_set_memory(PpuState *state, uint8_t *memory, size_t memory_size)
{
    if (!state)
        return;
    state->memory = memory;
    state->memory_size = memory_size;
}

static int memory_range_valid(const PpuState *state, uint64_t address, size_t length)
{
    return state->memory && address <= state->memory_size &&
           length <= state->memory_size - (size_t)address;
}

PpuStepResult ppu_step(PpuState *state)
{
    if (!state || !state->code || (state->pc & 3u) != 0 || state->pc > state->code_size ||
        state->code_size - (size_t)state->pc < 4)
        return PPU_STEP_BAD_ARGUMENT;

    const uint32_t insn = read_instruction_be(state->code + (size_t)state->pc);
    const uint32_t opcode = insn >> 26;
    const uint32_t rt = (insn >> 21) & 31u;
    const uint32_t ra = (insn >> 16) & 31u;
    const uint16_t imm = (uint16_t)(insn & 0xffffu);
    const int64_t signed_imm = (int16_t)imm;
    const uint64_t base = ra == 0 ? 0 : state->gpr[ra];
    const uint64_t address = base + (uint64_t)signed_imm;

    switch (opcode) {
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
    case 32: /* lwz */
        if ((address & 3u) != 0 || !memory_range_valid(state, address, 4))
            return PPU_STEP_MEMORY_FAULT;
        state->gpr[rt] = ((uint32_t)state->memory[address] << 24) |
                         ((uint32_t)state->memory[address + 1] << 16) |
                         ((uint32_t)state->memory[address + 2] << 8) |
                         (uint32_t)state->memory[address + 3];
        break;
    case 34: /* lbz */
        if (!memory_range_valid(state, address, 1)) return PPU_STEP_MEMORY_FAULT;
        state->gpr[rt] = state->memory[address];
        break;
    case 36: /* stw */
        if ((address & 3u) != 0 || !memory_range_valid(state, address, 4))
            return PPU_STEP_MEMORY_FAULT;
        state->memory[address] = (uint8_t)(state->gpr[rt] >> 24);
        state->memory[address + 1] = (uint8_t)(state->gpr[rt] >> 16);
        state->memory[address + 2] = (uint8_t)(state->gpr[rt] >> 8);
        state->memory[address + 3] = (uint8_t)state->gpr[rt];
        break;
    case 38: /* stb */
        if (!memory_range_valid(state, address, 1)) return PPU_STEP_MEMORY_FAULT;
        state->memory[address] = (uint8_t)state->gpr[rt];
        break;
    default:
        return PPU_STEP_UNSUPPORTED;
    }

    state->pc += 4;
    return PPU_STEP_OK;
}
