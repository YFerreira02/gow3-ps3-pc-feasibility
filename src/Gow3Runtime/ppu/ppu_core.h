#ifndef GOW3_PPU_CORE_H
#define GOW3_PPU_CORE_H

#include <stddef.h>
#include <stdint.h>

typedef enum PpuStepResult {
    PPU_STEP_OK = 0,
    PPU_STEP_UNSUPPORTED = 1,
    PPU_STEP_BAD_ARGUMENT = 2,
    PPU_STEP_MEMORY_FAULT = 3,
    PPU_STEP_SYSCALL = 4
} PpuStepResult;

typedef struct PpuState {
    uint64_t gpr[32];
    uint64_t pc;
    uint64_t lr;
    uint64_t ctr;
    uint32_t cr;
    uint8_t xer_so;
    const uint8_t *code;
    size_t code_size;
    uint64_t code_base;
    uint8_t *memory;
    size_t memory_size;
    uint64_t memory_base;
} PpuState;

void ppu_state_init(PpuState *state, const uint8_t *code, size_t code_size, uint64_t entry_pc);
void ppu_set_code_base(PpuState *state, uint64_t code_base);
void ppu_set_memory(PpuState *state, uint8_t *memory, size_t memory_size);
void ppu_set_memory_at(PpuState *state, uint8_t *memory, size_t memory_size, uint64_t guest_base);
PpuStepResult ppu_step(PpuState *state);

#endif
