#ifndef GOW3_PPU_CORE_H
#define GOW3_PPU_CORE_H

#include <stddef.h>
#include <stdint.h>

typedef enum PpuStepResult {
    PPU_STEP_OK = 0,
    PPU_STEP_UNSUPPORTED = 1,
    PPU_STEP_BAD_ARGUMENT = 2,
    PPU_STEP_MEMORY_FAULT = 3,
    PPU_STEP_SYSCALL = 4,
    PPU_STEP_LIMIT_REACHED = 5
} PpuStepResult;

typedef struct PpuState {
    uint64_t gpr[32];
    double fpr[32];
    uint32_t fpr_integer_word[32];
    uint8_t fpr_integer_word_valid[32];
    uint64_t pc;
    uint64_t lr;
    uint64_t ctr;
    uint32_t cr;
    uint8_t xer_so;
    uint8_t xer_ov;
    uint8_t xer_ca;
    uint8_t xer_bc;
    uint64_t reservation_address;
    uint32_t reservation_raw_value;
    uint8_t reservation_valid;
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
PpuStepResult ppu_run(PpuState *state, uint64_t instruction_budget, uint64_t *executed);

#endif
