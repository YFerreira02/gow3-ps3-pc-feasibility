#include "ppu_core.h"


static uint32_t read_instruction_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint16_t read_u16_be(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read_u32_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void write_u16_be(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static void write_u32_be(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
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

static uint64_t rotate_left_doubleword(uint64_t value, uint32_t shift)
{
    shift &= 63u;
    return shift == 0 ? value : (value << shift) | (value >> (64u - shift));
}

static uint64_t rotate_mask_doubleword(uint32_t begin, uint32_t end)
{
    uint64_t mask = 0;
    for (uint32_t bit = 0; bit < 64u; ++bit) {
        const uint32_t ppc_bit = 63u - bit;
        const int selected = begin <= end
            ? (ppc_bit >= begin && ppc_bit <= end)
            : (ppc_bit >= begin || ppc_bit <= end);
        if (selected)
            mask |= 1ull << bit;
    }
    return mask;
}

static uint32_t population_count32(uint32_t value)
{
    uint32_t count = 0;
    while (value != 0) {
        value &= value - 1u;
        ++count;
    }
    return count;
}

static uint32_t population_count64(uint64_t value)
{
    uint32_t count = 0;
    while (value != 0) {
        value &= value - 1ull;
        ++count;
    }
    return count;
}

static uint64_t multiply_high_unsigned(uint64_t left, uint64_t right)
{
    const uint64_t left_low = (uint32_t)left;
    const uint64_t left_high = left >> 32;
    const uint64_t right_low = (uint32_t)right;
    const uint64_t right_high = right >> 32;
    const uint64_t low_product = left_low * right_low;
    const uint64_t middle = left_high * right_low + (low_product >> 32);
    const uint64_t upper_middle = left_low * right_high + (middle & 0xffffffffu);
    return left_high * right_high + (middle >> 32) + (upper_middle >> 32);
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

static void write_fpr_double(PpuState *state, uint32_t reg, double value)
{
    state->fpr[reg] = value;
    state->fpr_integer_word_valid[reg] = 0;
}

static uint64_t fpr_storage_bits(const PpuState *state, uint32_t reg)
{
    return state->fpr_integer_word_valid[reg]
        ? (uint64_t)state->fpr_integer_word[reg]
        : double_bits(state->fpr[reg]);
}

static double round_to_integer(double value, uint32_t mode)
{
    const uint64_t bits = double_bits(value);
    const uint64_t sign = bits & 0x8000000000000000ull;
    const uint64_t magnitude = bits & 0x7fffffffffffffffull;
    const uint32_t exponent = (uint32_t)((magnitude >> 52) & 0x7ffu);
    if (exponent == 0x7ffu || magnitude == 0)
        return value;
    const int32_t unbiased = (int32_t)exponent - 1023;
    if (unbiased >= 52)
        return value;
    if (unbiased < 0) {
        int round_away = 0;
        if (mode == 2u) round_away = sign == 0; /* toward +infinity */
        else if (mode == 3u) round_away = sign != 0; /* toward -infinity */
        else if (mode == 0u && unbiased == -1 && (magnitude & 0x000fffffffffffffull) != 0)
            round_away = 1; /* nearest, ties to even */
        return double_from_bits(sign | (round_away ? 0x3ff0000000000000ull : 0));
    }
    const uint32_t fractional_bits = 52u - (uint32_t)unbiased;
    const uint64_t fraction_mask = (1ull << fractional_bits) - 1ull;
    const uint64_t remainder = magnitude & fraction_mask;
    if (remainder == 0)
        return value;
    int increment = mode == 2u ? sign == 0 : mode == 3u ? sign != 0 : 0;
    if (mode == 0u) {
        const uint64_t halfway = 1ull << (fractional_bits - 1u);
        increment = remainder > halfway ||
            (remainder == halfway && ((magnitude >> fractional_bits) & 1u) != 0);
    }
    uint64_t rounded = magnitude & ~fraction_mask;
    if (increment)
        rounded += 1ull << fractional_bits;
    return double_from_bits(sign | rounded);
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
    volatile uint8_t *vector_bytes = &state->vr[0][0];
    for (size_t byte = 0; byte < 32u * 16u; ++byte)
        vector_bytes[byte] = 0;
    volatile double *fpr = state->fpr;
    for (size_t i = 0; i < 32; ++i) {
        fpr[i] = 0.0;
        state->fpr_integer_word[i] = 0;
        state->fpr_integer_word_valid[i] = 0;
    }
    state->lr = 0;
    state->ctr = 0;
    state->cr = 0;
    state->xer_so = 0;
    state->xer_ov = 0;
    state->xer_ca = 0;
    state->xer_bc = 0;
    state->reservation_address = 0;
    state->reservation_raw_value = 0;
    state->reservation_valid = 0;
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

/* ---- VMX/AltiVec helpers ------------------------------------------------
 * Vector registers are stored as 16 bytes in guest (big-endian) element order:
 * byte 0 is element 0. The Cell PPE VMX unit runs in non-Java mode, so
 * single-precision denormal inputs and results are flushed to signed zero. */

static uint32_t vr_word(const PpuState *state, uint32_t reg, uint32_t index)
{
    return read_u32_be(state->vr[reg] + index * 4u);
}

static float vmx_flush_input(float value)
{
    const uint32_t bits = float_bits(value);
    if ((bits & 0x7f800000u) == 0 && (bits & 0x007fffffu) != 0)
        return float_from_bits(bits & 0x80000000u);
    return value;
}

static uint32_t vmx_flush_result(float value)
{
    uint32_t bits = float_bits(value);
    if ((bits & 0x7f800000u) == 0)
        return bits & 0x80000000u;
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0)
        bits |= 0x00400000u; /* results are quiet NaNs */
    return bits;
}

static int vmx_is_nan(float value)
{
    const uint32_t bits = float_bits(value);
    return (bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0;
}

static float vmx_lane_float(const PpuState *state, uint32_t reg, uint32_t index)
{
    return vmx_flush_input(float_from_bits(vr_word(state, reg, index)));
}

static void vmx_set_cr6(PpuState *state, int all_true, int all_false)
{
    const uint32_t field = (all_true ? 8u : 0u) | (all_false ? 2u : 0u);
    const uint32_t shift = (7u - 6u) * 4u;
    state->cr = (state->cr & ~(0xfu << shift)) | (field << shift);
}

static PpuStepResult execute_vmx(PpuState *state, uint32_t insn)
{
    const uint32_t vd = (insn >> 21) & 31u;
    const uint32_t va = (insn >> 16) & 31u;
    const uint32_t vb = (insn >> 11) & 31u;
    const uint32_t vc = (insn >> 6) & 31u;
    const uint32_t va_xo = insn & 0x3fu;
    uint8_t out[16];

    /* VA-form instructions use a 6-bit extended opcode and a fourth register. */
    if (va_xo >= 42u && va_xo <= 47u && va_xo != 45u) {
        if (va_xo == 42u) { /* vsel */
            for (uint32_t i = 0; i < 16; ++i)
                out[i] = (uint8_t)((state->vr[va][i] & ~state->vr[vc][i]) |
                                   (state->vr[vb][i] & state->vr[vc][i]));
        } else if (va_xo == 43u) { /* vperm */
            uint8_t source[32];
            for (uint32_t i = 0; i < 16; ++i) {
                source[i] = state->vr[va][i];
                source[16 + i] = state->vr[vb][i];
            }
            for (uint32_t i = 0; i < 16; ++i)
                out[i] = source[state->vr[vc][i] & 31u];
        } else if (va_xo == 44u) { /* vsldoi */
            if ((insn & (1u << 10)) != 0)
                return PPU_STEP_UNSUPPORTED; /* reserved bit must be zero */
            const uint32_t shift = vc & 15u;
            for (uint32_t i = 0; i < 16; ++i) {
                const uint32_t index = i + shift;
                out[i] = index < 16u ? state->vr[va][index] : state->vr[vb][index - 16u];
            }
        } else { /* 46 vmaddfp, 47 vnmsubfp: (A * C) +/- B */
            for (uint32_t i = 0; i < 4; ++i) {
                const float a = vmx_lane_float(state, va, i);
                const float b = vmx_lane_float(state, vb, i);
                const float c = vmx_lane_float(state, vc, i);
                uint32_t bits;
                if (vmx_is_nan(a) || vmx_is_nan(b) || vmx_is_nan(c)) {
                    const float nan = vmx_is_nan(a) ? a : (vmx_is_nan(b) ? b : c);
                    bits = float_bits(nan) | 0x00400000u;
                } else {
                    /* The single-precision product is exact in double precision;
                     * the final double->float conversion can, in rare halfway
                     * cases, differ from a true fused single rounding. */
                    const double product = (double)a * (double)c;
                    double value = va_xo == 46u ? product + (double)b : -(product - (double)b);
                    bits = vmx_flush_result((float)value);
                }
                write_u32_be(out + i * 4u, bits);
            }
        }
        for (uint32_t i = 0; i < 16; ++i)
            state->vr[vd][i] = out[i];
        state->pc += 4;
        return PPU_STEP_OK;
    }

    const uint32_t vxo = insn & 0x7ffu;
    const uint32_t compare_xo = insn & 0x3ffu;
    const int compare_record = (insn & 0x400u) != 0;

    /* VXR-form compares: bit 0x400 is Rc and updates CR6. */
    if (compare_xo == 6u || compare_xo == 70u || compare_xo == 134u ||
        compare_xo == 198u || compare_xo == 454u || compare_xo == 710u) {
        int all_true = 1, all_false = 1;
        if (compare_xo == 6u || compare_xo == 70u || compare_xo == 134u) {
            const uint32_t width = compare_xo == 6u ? 1u : (compare_xo == 70u ? 2u : 4u);
            for (uint32_t i = 0; i < 16u; i += width) {
                int equal = 1;
                for (uint32_t j = 0; j < width; ++j)
                    equal &= state->vr[va][i + j] == state->vr[vb][i + j];
                /* Write the lane mask with sized stores; a byte loop here is
                 * turned into a memset call, which the -nostdlib build lacks. */
                if (width == 1u) out[i] = equal ? 0xffu : 0u;
                else if (width == 2u) write_u16_be(out + i, equal ? 0xffffu : 0u);
                else write_u32_be(out + i, equal ? 0xffffffffu : 0u);
                all_true &= equal;
                all_false &= !equal;
            }
        } else {
            for (uint32_t i = 0; i < 4; ++i) {
                const float a = vmx_lane_float(state, va, i);
                const float b = vmx_lane_float(state, vb, i);
                int result;
                if (compare_xo == 198u) result = a == b;       /* vcmpeqfp */
                else if (compare_xo == 454u) result = a >= b;  /* vcmpgefp */
                else result = a > b;                           /* vcmpgtfp */
                write_u32_be(out + i * 4u, result ? 0xffffffffu : 0u);
                all_true &= result;
                all_false &= !result;
            }
        }
        for (uint32_t i = 0; i < 16; ++i)
            state->vr[vd][i] = out[i];
        if (compare_record)
            vmx_set_cr6(state, all_true, all_false);
        state->pc += 4;
        return PPU_STEP_OK;
    }

    switch (vxo) {
    case 0u: /* vaddubm */
    case 1024u: /* vsububm */
        for (uint32_t i = 0; i < 16; ++i)
            out[i] = (uint8_t)(vxo == 0u ? state->vr[va][i] + state->vr[vb][i]
                                         : state->vr[va][i] - state->vr[vb][i]);
        break;
    case 64u: /* vadduhm */
    case 1088u: /* vsubuhm */
        for (uint32_t i = 0; i < 8; ++i) {
            const uint16_t a = read_u16_be(state->vr[va] + i * 2u);
            const uint16_t b = read_u16_be(state->vr[vb] + i * 2u);
            write_u16_be(out + i * 2u, (uint16_t)(vxo == 64u ? a + b : a - b));
        }
        break;
    case 128u: /* vadduwm */
    case 1152u: /* vsubuwm */
        for (uint32_t i = 0; i < 4; ++i) {
            const uint32_t a = vr_word(state, va, i);
            const uint32_t b = vr_word(state, vb, i);
            write_u32_be(out + i * 4u, vxo == 128u ? a + b : a - b);
        }
        break;
    case 1028u: case 1092u: case 1156u: case 1220u: case 1284u:
        for (uint32_t i = 0; i < 16; ++i) {
            const uint8_t a = state->vr[va][i], b = state->vr[vb][i];
            if (vxo == 1028u) out[i] = (uint8_t)(a & b);           /* vand */
            else if (vxo == 1092u) out[i] = (uint8_t)(a & ~b);     /* vandc */
            else if (vxo == 1156u) out[i] = (uint8_t)(a | b);      /* vor */
            else if (vxo == 1220u) out[i] = (uint8_t)(a ^ b);      /* vxor */
            else out[i] = (uint8_t)~(a | b);                       /* vnor */
        }
        break;
    case 524u: case 588u: case 652u: { /* vspltb, vsplth, vspltw */
        const uint32_t width = vxo == 524u ? 1u : (vxo == 588u ? 2u : 4u);
        const uint32_t element = va & (16u / width - 1u);
        for (uint32_t i = 0; i < 16u; ++i)
            out[i] = state->vr[vb][element * width + (i % width)];
        break;
    }
    case 780u: case 844u: case 908u: { /* vspltisb, vspltish, vspltisw */
        if (vb != 0)
            return PPU_STEP_UNSUPPORTED;
        const int32_t simm = (int32_t)(va << 27) >> 27;
        for (uint32_t i = 0; i < 16u; ++i) {
            if (vxo == 780u) out[i] = (uint8_t)simm;
            else if (vxo == 844u) out[i] = (uint8_t)((uint32_t)simm >> ((i & 1u) ? 0u : 8u));
            else out[i] = (uint8_t)((uint32_t)simm >> (24u - (i & 3u) * 8u));
        }
        break;
    }
    case 12u: case 76u: case 140u: case 268u: case 332u: case 396u: {
        /* vmrghb/vmrghh/vmrghw and vmrglb/vmrglh/vmrglw */
        const uint32_t width = (vxo == 12u || vxo == 268u) ? 1u
                             : ((vxo == 76u || vxo == 332u) ? 2u : 4u);
        const uint32_t half = vxo >= 268u ? 8u : 0u;
        for (uint32_t pair = 0; pair < 8u / width; ++pair) {
            for (uint32_t j = 0; j < width; ++j) {
                out[pair * 2u * width + j] = state->vr[va][half + pair * width + j];
                out[pair * 2u * width + width + j] = state->vr[vb][half + pair * width + j];
            }
        }
        break;
    }
    case 388u: case 644u: case 900u: /* vslw, vsrw, vsraw */
        for (uint32_t i = 0; i < 4; ++i) {
            const uint32_t a = vr_word(state, va, i);
            const uint32_t shift = vr_word(state, vb, i) & 31u;
            uint32_t r;
            if (vxo == 388u) r = a << shift;
            else if (vxo == 644u) r = a >> shift;
            else r = (uint32_t)((int32_t)a >> shift);
            write_u32_be(out + i * 4u, r);
        }
        break;
    case 10u: case 74u: case 1034u: case 1098u: /* vaddfp, vsubfp, vmaxfp, vminfp */
        for (uint32_t i = 0; i < 4; ++i) {
            const float a = vmx_lane_float(state, va, i);
            const float b = vmx_lane_float(state, vb, i);
            uint32_t bits;
            if (vmx_is_nan(a) || vmx_is_nan(b)) {
                bits = float_bits(vmx_is_nan(a) ? a : b) | 0x00400000u;
            } else if (vxo == 10u) {
                bits = vmx_flush_result(a + b);
            } else if (vxo == 74u) {
                bits = vmx_flush_result(a - b);
            } else {
                /* max(+0,-0)=+0 and min(+0,-0)=-0 per the VMX definition. */
                if (a == b)
                    bits = vxo == 1034u ? (float_bits(a) & float_bits(b))
                                        : (float_bits(a) | float_bits(b));
                else
                    bits = float_bits(vxo == 1034u ? (a > b ? a : b) : (a < b ? a : b));
            }
            write_u32_be(out + i * 4u, bits);
        }
        break;
    default:
        return PPU_STEP_UNSUPPORTED;
    }
    for (uint32_t i = 0; i < 16; ++i)
        state->vr[vd][i] = out[i];
    state->pc += 4;
    return PPU_STEP_OK;
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
    case 4: /* selected VMX/AltiVec vector operations */
        return execute_vmx(state, insn);
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
    case 8: { /* subfic: signed immediate minus RA, carry means no borrow */
        const uint64_t minuend = (uint64_t)signed_imm;
        const uint64_t subtrahend = state->gpr[ra];
        state->gpr[rt] = minuend - subtrahend;
        state->xer_ca = (uint8_t)(minuend >= subtrahend);
        break;
    }
    case 12: case 13: { /* addic / addic. */
        const uint64_t source = state->gpr[ra];
        const uint64_t addend = (uint64_t)signed_imm;
        state->gpr[rt] = source + addend;
        state->xer_ca = (uint8_t)(state->gpr[rt] < source);
        if (opcode == 13u)
            set_cr0_from_result(state, state->gpr[rt]);
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
    case 30: { /* MD-form rotate doubleword immediate operations */
        const uint32_t sh = ((insn >> 11) & 31u) | ((insn & 2u) << 4);
        const uint32_t mb = (insn >> 5) & 63u;
        const uint32_t xo = (insn >> 2) & 7u;
        const int record = (insn & 1u) != 0;
        const uint64_t rotated = rotate_left_doubleword(state->gpr[rt], sh);
        uint32_t begin, end;
        if (xo == 0u) { begin = mb; end = 63u; } /* rldicl */
        else if (xo == 1u) { begin = 0u; end = mb; } /* rldicr */
        else if (xo == 2u || xo == 3u) { begin = mb; end = (63u - sh) & 63u; }
        else return PPU_STEP_UNSUPPORTED;
        const uint64_t mask = rotate_mask_doubleword(begin, end);
        const uint64_t result = xo == 3u
            ? ((state->gpr[ra] & ~mask) | (rotated & mask))
            : (rotated & mask);
        state->gpr[ra] = result;
        if (record)
            set_cr0_from_result(state, result);
        state->pc += 4;
        return PPU_STEP_OK;
    }
    case 46: case 47: { /* lmw / stmw: transfer GPRs RT through r31 */
        const size_t transfer_size = (size_t)(32u - rt) * 4u;
        size_t memory_offset;
        if (!address_valid || (address & 3u) != 0 ||
            !memory_range_offset(state, address, transfer_size, &memory_offset))
            return PPU_STEP_MEMORY_FAULT;
        for (uint32_t reg = rt; reg < 32u; ++reg) {
            const size_t offset = memory_offset + (size_t)(reg - rt) * 4u;
            if (opcode == 46u) {
                state->gpr[reg] = ((uint32_t)state->memory[offset] << 24) |
                    ((uint32_t)state->memory[offset + 1] << 16) |
                    ((uint32_t)state->memory[offset + 2] << 8) |
                    (uint32_t)state->memory[offset + 3];
            } else {
                const uint32_t value = (uint32_t)state->gpr[reg];
                state->memory[offset] = (uint8_t)(value >> 24);
                state->memory[offset + 1] = (uint8_t)(value >> 16);
                state->memory[offset + 2] = (uint8_t)(value >> 8);
                state->memory[offset + 3] = (uint8_t)value;
            }
        }
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
        if (xo == 150u) { /* isync: synchronize instruction stream */
            if ((insn & 1u) != 0)
                return PPU_STEP_UNSUPPORTED;
            __atomic_thread_fence(__ATOMIC_SEQ_CST);
            state->pc = next_pc;
            return PPU_STEP_OK;
        }
        if (xo == 0u) { /* mcrf: move one condition-register field */
            const uint32_t bf = (insn >> 23) & 7u;
            const uint32_t bfa = (insn >> 18) & 7u;
            if ((insn & 1u) != 0)
                return PPU_STEP_UNSUPPORTED;
            const uint32_t source_shift = 28u - (bfa * 4u);
            const uint32_t target_shift = 28u - (bf * 4u);
            const uint32_t value = (state->cr >> source_shift) & 0x0fu;
            state->cr = (state->cr & ~(0x0fu << target_shift)) | (value << target_shift);
            state->pc = next_pc;
            return PPU_STEP_OK;
        }
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
            write_fpr_double(state, rt, (double)(float)state->fpr[frb]);
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo10 == 15u) { /* fctiwz: signed word result occupies low FPR word */
            const double source = state->fpr[frb];
            int32_t converted;
            if (source != source || source < -2147483648.0)
                converted = INT32_MIN;
            else if (source > 2147483647.0)
                converted = INT32_MAX;
            else
                converted = (int32_t)source;
            state->fpr[rt] = (double)converted;
            state->fpr_integer_word[rt] = (uint32_t)converted;
            state->fpr_integer_word_valid[rt] = 1;
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo10 == 392u || xo10 == 424u || xo10 == 456u || xo10 == 488u) {
            const uint32_t mode = xo10 == 392u ? 0u : xo10 == 424u ? 1u :
                                  xo10 == 456u ? 2u : 3u;
            write_fpr_double(state, rt, round_to_integer(state->fpr[frb], mode)); /* frin/z/p/m */
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo5 == 23u) { /* fsel: choose FRC for nonnegative FRA, otherwise FRB */
            const uint32_t frc = (insn >> 6) & 31u;
            write_fpr_double(state, rt, state->fpr[ra] >= 0.0 ? state->fpr[frc] : state->fpr[frb]);
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo5 == 21u) write_fpr_double(state, rt, state->fpr[ra] + state->fpr[frb]); /* fadd */
        else if (xo5 == 20u) write_fpr_double(state, rt, state->fpr[ra] - state->fpr[frb]); /* fsub */
        else if (xo5 == 25u) write_fpr_double(state, rt, state->fpr[ra] * state->fpr[(insn >> 6) & 31u]); /* fmul */
        else if (xo5 == 18u) {
            const double divisor = state->fpr[frb];
            if (divisor == 0.0) return PPU_STEP_UNSUPPORTED;
            write_fpr_double(state, rt, state->fpr[ra] / divisor); /* fdiv */
        } else if (xo10 == 72u) write_fpr_double(state, rt, state->fpr[frb]); /* fmr */
        else if (xo10 == 264u) write_fpr_double(state, rt, double_from_bits(double_bits(state->fpr[frb]) & 0x7fffffffffffffffull)); /* fabs */
        else if (xo10 == 136u) write_fpr_double(state, rt, double_from_bits(double_bits(state->fpr[frb]) | 0x8000000000000000ull)); /* fnabs */
        else if (xo10 == 40u) write_fpr_double(state, rt, double_from_bits(double_bits(state->fpr[frb]) ^ 0x8000000000000000ull)); /* fneg */
        else return PPU_STEP_UNSUPPORTED;
        state->pc += 4;
        return PPU_STEP_OK;
    }
    case 31: { /* selected common X-form integer instructions */
        const uint32_t rb = (insn >> 11) & 31u;
        const uint32_t xo = (insn >> 1) & 0x3ffu;
        const int record = (insn & 1u) != 0;
        uint64_t result;
        if (xo == 103u || xo == 359u || xo == 231u || xo == 487u) {
            const int is_load = xo == 103u || xo == 359u;
            const uint64_t indexed_base = ra == 0 ? 0 : state->gpr[ra];
            if (record || indexed_base > UINT64_MAX - state->gpr[rb])
                return record ? PPU_STEP_UNSUPPORTED : PPU_STEP_MEMORY_FAULT;
            const uint64_t vector_address = (indexed_base + state->gpr[rb]) & ~15ull;
            size_t memory_offset;
            if (!memory_range_offset(state, vector_address, 16u, &memory_offset))
                return PPU_STEP_MEMORY_FAULT;
            for (size_t i = 0; i < 16u; ++i) {
                if (is_load)
                    state->vr[rt][i] = state->memory[memory_offset + i];
                else
                    state->memory[memory_offset + i] = state->vr[rt][i];
            }
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo == 6u || xo == 38u) { /* lvsl / lvsr: permute-control vectors */
            const uint64_t indexed_base = ra == 0 ? 0 : state->gpr[ra];
            if (record)
                return PPU_STEP_UNSUPPORTED;
            const uint32_t shift = (uint32_t)((indexed_base + state->gpr[rb]) & 15u);
            for (uint32_t i = 0; i < 16u; ++i)
                state->vr[rt][i] = (uint8_t)(xo == 6u ? shift + i : 16u - shift + i);
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo == 598u || xo == 854u) { /* sync / eieio ordering barriers */
            if (record)
                return PPU_STEP_UNSUPPORTED;
            __atomic_thread_fence(__ATOMIC_SEQ_CST);
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo == 54u || xo == 86u || xo == 246u || xo == 278u ||
            xo == 470u || xo == 758u || xo == 982u) {
            /* dcbst, dcbf, dcbtst, dcbt, dcbi, dst, icbi: no host cache exists. */
            if (record)
                return PPU_STEP_UNSUPPORTED;
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo == 1014u) { /* dcbz: clear the Cell PPE's 128-byte cache block */
            if (record || rt != 0)
                return PPU_STEP_UNSUPPORTED;
            const uint64_t indexed_base = ra == 0 ? 0 : state->gpr[ra];
            if (indexed_base > UINT64_MAX - state->gpr[rb])
                return PPU_STEP_MEMORY_FAULT;
            const uint64_t block_address = (indexed_base + state->gpr[rb]) & ~127ull;
            size_t memory_offset;
            if (!memory_range_offset(state, block_address, 128u, &memory_offset))
                return PPU_STEP_MEMORY_FAULT;
            for (size_t i = 0; i < 128u; ++i)
                state->memory[memory_offset + i] = 0;
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo == 20u) { /* lwarx: atomic word load with reservation */
            if (record)
                return PPU_STEP_UNSUPPORTED;
            const uint64_t indexed_base = ra == 0 ? 0 : state->gpr[ra];
            if (indexed_base > UINT64_MAX - state->gpr[rb])
                return PPU_STEP_MEMORY_FAULT;
            const uint64_t effective_address = indexed_base + state->gpr[rb];
            size_t memory_offset;
            if ((effective_address & 3u) != 0 ||
                !memory_range_offset(state, effective_address, 4, &memory_offset))
                return PPU_STEP_MEMORY_FAULT;
            uint32_t raw = __atomic_load_n((uint32_t *)(void *)(state->memory + memory_offset),
                                           __ATOMIC_SEQ_CST);
            state->reservation_address = effective_address;
            state->reservation_raw_value = raw;
            state->reservation_valid = 1;
            state->gpr[rt] = ((uint64_t)(raw & 0xffu) << 24) |
                ((uint64_t)((raw >> 8) & 0xffu) << 16) |
                ((uint64_t)((raw >> 16) & 0xffu) << 8) | (uint64_t)(raw >> 24);
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo == 150u) { /* stwcx.: conditional atomic store and CR0 status */
            if (!record)
                return PPU_STEP_UNSUPPORTED;
            const uint64_t indexed_base = ra == 0 ? 0 : state->gpr[ra];
            if (indexed_base > UINT64_MAX - state->gpr[rb])
                return PPU_STEP_MEMORY_FAULT;
            const uint64_t effective_address = indexed_base + state->gpr[rb];
            size_t memory_offset;
            if ((effective_address & 3u) != 0 ||
                !memory_range_offset(state, effective_address, 4, &memory_offset))
                return PPU_STEP_MEMORY_FAULT;
            int succeeded = 0;
            if (state->reservation_valid && state->reservation_address == effective_address) {
                const uint32_t value = (uint32_t)state->gpr[rt];
                const uint32_t desired_raw = ((value & 0xffu) << 24) |
                    ((value & 0xff00u) << 8) | ((value >> 8) & 0xff00u) | (value >> 24);
                uint32_t expected_raw = state->reservation_raw_value;
                succeeded = __atomic_compare_exchange_n(
                    (uint32_t *)(void *)(state->memory + memory_offset), &expected_raw,
                    desired_raw, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
            }
            state->reservation_valid = 0;
            set_cr_field(state, 0, succeeded ? 1u : 0u);
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if ((xo & 31u) == 15u) { /* isel uses an A-form with BC in XO's upper bits */
            const uint32_t condition_bit = (insn >> 6) & 31u;
            if (record)
                return PPU_STEP_UNSUPPORTED;
            result = (state->cr & (1u << (31u - condition_bit))) != 0
                ? (ra == 0 ? 0 : state->gpr[ra])
                : state->gpr[rb];
            state->gpr[rt] = result;
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo == 27u || xo == 539u || xo == 794u) { /* sld / srd / srad */
            const uint64_t shift_field = state->gpr[rb];
            const uint32_t shift = (uint32_t)(shift_field & 0x7fu);
            const uint64_t source = state->gpr[rt];
            if (xo == 27u)
                result = (shift_field & ~63ull) != 0 ? 0 : source << shift;
            else if (xo == 539u)
                result = (shift_field & ~63ull) != 0 ? 0 : source >> shift;
            else {
                const int negative = (source >> 63) != 0;
                if (shift == 0) {
                    result = source;
                    state->xer_ca = 0;
                } else if (shift >= 64u) {
                    result = negative ? UINT64_MAX : 0;
                    state->xer_ca = (uint8_t)negative;
                } else {
                    result = source >> shift;
                    if (negative)
                        result |= UINT64_MAX << (64u - shift);
                    const uint64_t discarded = source & ((1ull << shift) - 1ull);
                    state->xer_ca = (uint8_t)(negative && discarded != 0);
                }
            }
            state->gpr[ra] = result;
            if (record)
                set_cr0_from_result(state, result);
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo == 792u || xo == 824u) { /* sraw / srawi */
            const int immediate = xo == 824u;
            const uint32_t shift = immediate ? rb : (uint32_t)state->gpr[rb] & 0x3fu;
            const uint32_t source = (uint32_t)state->gpr[rt];
            const uint32_t sign_fill = (source & 0x80000000u) ? UINT32_MAX : 0u;
            uint32_t shifted;
            if (shift == 0) {
                shifted = source;
                state->xer_ca = 0;
            } else if (shift >= 32u) {
                shifted = sign_fill;
                state->xer_ca = (uint8_t)(sign_fill != 0);
            } else {
                shifted = source >> shift;
                if (sign_fill != 0)
                    shifted |= UINT32_MAX << (32u - shift);
                const uint32_t discarded = source & ((1u << shift) - 1u);
                state->xer_ca = (uint8_t)(sign_fill != 0 && discarded != 0);
            }
            state->gpr[ra] = (uint64_t)(int64_t)(int32_t)shifted;
            if (record)
                set_cr0_from_result(state, state->gpr[ra]);
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo == 512u) { /* mcrxr: transfer XER summary bits to a CR field */
            const uint32_t bf = (insn >> 23) & 7u;
            if (record)
                return PPU_STEP_UNSUPPORTED;
            const uint32_t shift = 28u - (bf * 4u);
            const uint32_t field = ((uint32_t)(state->xer_so & 1u) << 3) |
                ((uint32_t)(state->xer_ov & 1u) << 2) |
                ((uint32_t)(state->xer_ca & 1u) << 1);
            state->cr = (state->cr & ~(0x0fu << shift)) | (field << shift);
            state->xer_so = 0;
            state->xer_ov = 0;
            state->xer_ca = 0;
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo == 533u || xo == 661u) { /* lswx / stswx */
            const int is_load = xo == 533u;
            const uint32_t byte_count = state->xer_bc & 0x7fu;
            if (record)
                return PPU_STEP_UNSUPPORTED;
            if (byte_count == 0) {
                state->pc += 4;
                return PPU_STEP_OK;
            }
            const uint32_t register_count = (byte_count + 3u) / 4u;
            int ra_overlaps = 0, rb_overlaps = 0;
            for (uint32_t i = 0; i < register_count; ++i) {
                const uint32_t reg = (rt + i) & 31u;
                if (ra != 0 && reg == ra) ra_overlaps = 1;
                if (reg == rb) rb_overlaps = 1;
            }
            if (is_load && (ra_overlaps || rb_overlaps || (ra == 0 && rt == 0)))
                return PPU_STEP_UNSUPPORTED;
            const uint64_t indexed_base = ra == 0 ? 0 : state->gpr[ra];
            if (indexed_base > UINT64_MAX - state->gpr[rb])
                return PPU_STEP_MEMORY_FAULT;
            const uint64_t string_address = indexed_base + state->gpr[rb];
            size_t memory_offset;
            if (!memory_range_offset(state, string_address, byte_count, &memory_offset))
                return PPU_STEP_MEMORY_FAULT;
            for (uint32_t byte = 0; byte < byte_count; ++byte) {
                const uint32_t reg = (rt + byte / 4u) & 31u;
                const uint32_t in_word = byte & 3u;
                const size_t offset = memory_offset + byte;
                if (is_load) {
                    if (in_word == 0)
                        state->gpr[reg] = 0;
                    state->gpr[reg] |= (uint64_t)state->memory[offset] << (24u - in_word * 8u);
                } else {
                    state->memory[offset] = (uint8_t)(state->gpr[reg] >> (24u - in_word * 8u));
                }
            }
            state->pc += 4;
            return PPU_STEP_OK;
        }
        if (xo == 535u || xo == 567u || xo == 599u || xo == 631u ||
            xo == 663u || xo == 695u || xo == 727u || xo == 759u) {
            const int is_load = xo == 535u || xo == 567u || xo == 599u || xo == 631u;
            const int is_single = xo == 535u || xo == 567u || xo == 663u || xo == 695u;
            const int update = xo == 567u || xo == 631u || xo == 695u || xo == 759u;
            const size_t width = is_single ? 4u : 8u;
            const uint64_t base_address = ra == 0 ? 0 : state->gpr[ra];
            const uint64_t index = state->gpr[rb];
            uint64_t effective_address;
            size_t memory_offset;
            if ((insn & 1u) != 0 || (update && ra == 0))
                return PPU_STEP_UNSUPPORTED;
            if (base_address > UINT64_MAX - index)
                return PPU_STEP_MEMORY_FAULT;
            effective_address = base_address + index;
            if ((effective_address & (width - 1u)) != 0 ||
                !memory_range_offset(state, effective_address, width, &memory_offset))
                return PPU_STEP_MEMORY_FAULT;
            if (is_load) {
                uint64_t value = 0;
                for (size_t i = 0; i < width; ++i)
                    value = (value << 8) | state->memory[memory_offset + i];
                write_fpr_double(state, rt, is_single
                    ? (double)float_from_bits((uint32_t)value)
                    : double_from_bits(value));
            } else {
                const uint64_t value = is_single
                    ? float_bits((float)state->fpr[rt])
                    : fpr_storage_bits(state, rt);
                for (size_t i = 0; i < width; ++i)
                    state->memory[memory_offset + i] =
                        (uint8_t)(value >> ((uint32_t)(width - 1u - i) * 8u));
            }
            if (update)
                state->gpr[ra] = effective_address;
            state->pc += 4;
            return PPU_STEP_OK;
        }
        {
            size_t width = 0;
            int is_load = 0, update = 0, sign_extend = 0, byte_reverse = 0;
            switch (xo) {
            case 534: width = 4; is_load = 1; byte_reverse = 1; break; /* lwbrx */
            case 790: width = 2; is_load = 1; byte_reverse = 1; break; /* lhbrx */
            case 532: width = 8; is_load = 1; byte_reverse = 1; break; /* ldbrx */
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
            case 662: width = 4; byte_reverse = 1; break; /* stwbrx */
            case 918: width = 2; byte_reverse = 1; break; /* sthbrx */
            case 660: width = 8; byte_reverse = 1; break; /* stdbrx */
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
                    for (size_t i = 0; i < width; ++i) {
                        if (byte_reverse)
                            loaded |= (uint64_t)state->memory[memory_offset + i] << (i * 8u);
                        else
                            loaded = (loaded << 8) | state->memory[memory_offset + i];
                    }
                    if (sign_extend && width == 2)
                        loaded = (uint64_t)(int64_t)(int16_t)loaded;
                    else if (sign_extend && width == 4)
                        loaded = (uint64_t)(int64_t)(int32_t)loaded;
                    state->gpr[rt] = loaded;
                } else {
                    for (size_t i = 0; i < width; ++i) {
                        const uint32_t shift = byte_reverse
                            ? (uint32_t)(i * 8u)
                            : (uint32_t)(width - 1u - i) * 8u;
                        state->memory[memory_offset + i] = (uint8_t)(state->gpr[rt] >> shift);
                    }
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
        if (xo == 339u || xo == 467u) { /* mfspr / mtspr, limited to XER, LR and CTR */
            const uint32_t spr = ((insn >> 16) & 31u) | (((insn >> 11) & 31u) << 5);
            if (record || (spr != 1u && spr != 8u && spr != 9u))
                return PPU_STEP_UNSUPPORTED;
            if (xo == 339u) {
                if (spr == 1u)
                    state->gpr[rt] = ((uint64_t)(state->xer_so & 1u) << 31) |
                        ((uint64_t)(state->xer_ov & 1u) << 30) |
                        ((uint64_t)(state->xer_ca & 1u) << 29) | (state->xer_bc & 0x7fu);
                else
                    state->gpr[rt] = spr == 8u ? state->lr : state->ctr;
            } else if (spr == 1u) {
                state->xer_so = (uint8_t)((state->gpr[rt] >> 31) & 1u);
                state->xer_ov = (uint8_t)((state->gpr[rt] >> 30) & 1u);
                state->xer_ca = (uint8_t)((state->gpr[rt] >> 29) & 1u);
                state->xer_bc = (uint8_t)(state->gpr[rt] & 0x7fu);
            } else if (spr == 8u)
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
        case 234: { /* addme */
            if ((insn & (1u << 10)) != 0) return PPU_STEP_UNSUPPORTED;
            const uint64_t carry_in = state->xer_ca & 1u;
            result = state->gpr[ra] + UINT64_MAX + carry_in;
            state->xer_ca = (uint8_t)(carry_in || state->gpr[ra] != 0);
            break;
        }
        case 200: { /* subfze */
            if ((insn & (1u << 10)) != 0) return PPU_STEP_UNSUPPORTED;
            const uint64_t carry_in = state->xer_ca & 1u;
            result = ~state->gpr[ra] + carry_in;
            state->xer_ca = (uint8_t)(carry_in && state->gpr[ra] == 0);
            break;
        }
        case 232: { /* subfme */
            if ((insn & (1u << 10)) != 0) return PPU_STEP_UNSUPPORTED;
            const uint64_t carry_in = state->xer_ca & 1u;
            result = ~state->gpr[ra] + UINT64_MAX + carry_in;
            state->xer_ca = (uint8_t)(carry_in || state->gpr[ra] != UINT64_MAX);
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
        case 58: { /* cntlzd */
            uint64_t value = state->gpr[ra];
            uint32_t count = 0;
            while (count < 64u && (value & 0x8000000000000000ull) == 0) {
                ++count;
                value <<= 1;
            }
            result = count;
            break;
        }
        case 122: { /* popcntb: count set bits independently in every byte */
            result = 0;
            for (uint32_t byte = 0; byte < 8u; ++byte) {
                const uint32_t count = population_count32((uint32_t)(state->gpr[ra] >> (byte * 8u)) & 0xffu);
                result |= (uint64_t)count << (byte * 8u);
            }
            break;
        }
        case 378: result = population_count32((uint32_t)state->gpr[ra]); break; /* popcntw */
        case 506: result = population_count64(state->gpr[ra]); break; /* popcntd */
        case 954: result = (uint64_t)(int64_t)(int8_t)state->gpr[ra]; break; /* extsb */
        case 922: result = (uint64_t)(int64_t)(int16_t)state->gpr[ra]; break; /* extsh */
        case 986: result = (uint64_t)(int64_t)(int32_t)state->gpr[ra]; break; /* extsw */
        case 233: result = state->gpr[ra] * state->gpr[rb]; break; /* mulld */
        case 235: result = (uint32_t)state->gpr[ra] * (uint32_t)state->gpr[rb]; break; /* mullw */
        case 9: result = multiply_high_unsigned(state->gpr[ra], state->gpr[rb]); break; /* mulhdu */
        case 11: result = ((uint32_t)state->gpr[ra] * (uint64_t)(uint32_t)state->gpr[rb]) >> 32; break; /* mulhwu */
        case 75: {
            const int64_t product = (int64_t)(int32_t)state->gpr[ra] * (int64_t)(int32_t)state->gpr[rb];
            result = (uint64_t)(int64_t)(int32_t)((uint64_t)product >> 32);
            break;
        } /* mulhw */
        case 73: {
            const uint64_t left = state->gpr[ra], right = state->gpr[rb];
            result = multiply_high_unsigned(left, right);
            if ((left >> 63) != 0) result -= right;
            if ((right >> 63) != 0) result -= left;
            break;
        } /* mulhd */
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
                : fpr_storage_bits(state, rt);
            for (size_t i = 0; i < width; ++i)
                state->memory[memory_offset + i] =
                    (uint8_t)(value >> ((uint32_t)(width - 1u - i) * 8u));
        } else {
            uint64_t value = 0;
            for (size_t i = 0; i < width; ++i)
                value = (value << 8) | state->memory[memory_offset + i];
            write_fpr_double(state, rt, is_single
                ? (double)float_from_bits((uint32_t)value)
                : double_from_bits(value));
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
        write_fpr_double(state, rt, (double)value);
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

PpuStepResult ppu_run_with_syscall_handler(PpuState *state, uint64_t instruction_budget,
                                           uint64_t *executed, PpuSyscallHandler handler,
                                           void *handler_context)
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
            if (!handler) {
                if (executed) *executed = count;
                return result;
            }
            const PpuStepResult dispatch_result = handler(state, handler_context);
            if (dispatch_result != PPU_STEP_OK) {
                if (executed) *executed = count;
                return dispatch_result;
            }
        }
    }
    if (executed) *executed = count;
    return PPU_STEP_LIMIT_REACHED;
}

PpuStepResult ppu_run(PpuState *state, uint64_t instruction_budget, uint64_t *executed)
{
    return ppu_run_with_syscall_handler(state, instruction_budget, executed, 0, 0);
}
