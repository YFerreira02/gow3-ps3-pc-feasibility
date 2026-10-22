/* The four SPU ELFs embedded in the EBOOT (spu_0004..0007, lifted as images
 * 5..8) are Wws job modules wrapped in an ELF: the policy module streams
 * their segment into the job buffer at LS 0x4000 and enters at 0x4080, like
 * the 20 bare job modules (images 200..219). Registered only as workloads,
 * they were never recognised there, so all their code ran in the SPU
 * interpreter -- ~40 M interpreted instructions per 5 s in the intro
 * (SPU_INTERP_HIST, 2026-10-21), most of the SPU time. Signature = the first
 * 16 bytes of each segment (ELF file offset 0x80 = LS 0x4000). */
#include <stdint.h>

extern void spu_overlay_register_sig(const uint8_t sig[16], int image_id);

__attribute__((constructor)) static void gow3_spu_elf_overlays(void)
{
    static const uint8_t s4[16] = {0x43,0x7F,0xF8,0x02,0x43,0x02,0xDE,0x82,0x42,0x79,0x59,0x02,0x42,0x72,0xDD,0x82};
    static const uint8_t s5[16] = {0x43,0xB4,0xAE,0x02,0x43,0xA7,0x54,0x82,0x43,0x0D,0x43,0x02,0x42,0x3D,0xEB,0x82};
    static const uint8_t s6[16] = {0x43,0x99,0x18,0x02,0x43,0x36,0xBA,0x82,0x43,0x22,0x5F,0x02,0x43,0xAC,0xB7,0x82};
    static const uint8_t s7[16] = {0x43,0xFF,0x0C,0x02,0x43,0x9D,0xD2,0x82,0x42,0xF3,0x6D,0x02,0x43,0xEC,0x3D,0x82};
    spu_overlay_register_sig(s4, 5);   /* spu_0004_at_00493100 */
    spu_overlay_register_sig(s5, 6);   /* spu_0005_at_004A5B80 */
    spu_overlay_register_sig(s6, 7);   /* spu_0006_at_004C8A80 */
    spu_overlay_register_sig(s7, 8);   /* spu_0007_at_004DFA00 */
}
