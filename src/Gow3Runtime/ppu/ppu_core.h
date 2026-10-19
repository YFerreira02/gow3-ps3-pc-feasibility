#ifndef GOW3_PPU_CORE_H
#define GOW3_PPU_CORE_H

#include <stddef.h>
#include <stdint.h>

typedef enum PpuStepResult {
    PPU_STEP_OK = 0,
    PPU_STEP_UNSUPPORTED = 1,
    PPU_STEP_BAD_ARGUMENT = 2
} PpuStepResult;

typedef struct PpuState {
    uint64_t gpr[32];
    uint64_t pc;
    const uint8_t *code;
    size_t code_size;
} PpuState;

void ppu_state_init(PpuState *state, const uint8_t *code, size_t code_size, uint64_t entry_pc);
PpuStepResult ppu_step(PpuState *state);

#endif
