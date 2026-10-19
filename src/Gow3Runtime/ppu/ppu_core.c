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
    state->code = code;
    state->code_size = code_size;
    state->pc = entry_pc;
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
    default:
        return PPU_STEP_UNSUPPORTED;
    }

    state->pc += 4;
    return PPU_STEP_OK;
}
