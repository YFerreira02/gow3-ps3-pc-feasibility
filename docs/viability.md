# Standalone feasibility record / Registro de viabilidad

**Updated / Actualizado:** 2026-10-19
**Target / Objetivo:** Windows 11 x64; Ryzen 5 3500U (4C/8T); Radeon Vega 8; about 10 GB RAM; Vulkan 1.2 minimum.
**Goal / Meta:** adjustable resolution and 50–60 FPS; unverified.

## Scope / Alcance

Build an independent Windows runtime for the PS3 edition of *God of War III*. RPCS3 is excluded from the product. “Native” means a standalone Windows host application; PPU/SPU translation and PS3 service emulation remain necessary. The project is experimental and not playable.

Crear un runtime independiente de Windows para la versión PS3 de *God of War III*. RPCS3 queda fuera del producto. “Nativo” significa una aplicación Windows independiente; todavía hacen falta traducción PPU/SPU y emulación de servicios PS3. El proyecto es experimental y no es jugable.

## Existing host evidence / Evidencia del equipo

- The user-provided Task Manager images identify an AMD Ryzen 5 3500U with Radeon Vega Mobile graphics, plus a Radeon 530 Series as a second adapter. One snapshot showed roughly 92% system memory usage (9.1/9.9 GB).
- Earlier Vulkan probes in this repository reported Vulkan 1.2.170 on Vega 8 and passed synthetic instance/device/queue and triangle/offscreen checks.
- A prior offscreen clear/readback probe reported about 49–75 frames/s at 640×360 internal and 1280×720 output, ~33 at 960×540, and ~20 at 1280×720 internal across recorded runs. This was a synthetic graphics workload, not a game benchmark; don't use it to infer gameplay FPS.
- The Win32 swapchain probe had driver/presentation instability on the observed AMD setup. See git history and `tools/` probes for details.

## New standalone work / Nuevo trabajo independiente

`src/Gow3Runtime/ppu/` now contains a small C reference interpreter and a Windows x64 harness. In addition to single-step execution, `ppu_run` executes to a bounded instruction budget and yields at a syscall so a future host dispatcher can take over. Supported instructions include D-form immediate arithmetic/logical/compare operations (`mulli`, `addi`, `addis`, `addic`, `subfic`, immediate logical ops, `cmpi`, `cmpli`, `lmw`, `stmw`), register comparisons (`cmp`, `cmpl` with 32/64-bit signed/unsigned modes), selected X-form operations (`isel`, `add`, `addc`, `adde`, `addme`, `addze`, `subf`, `subfc`, `subfe`, `subfme`, `subfze`, `mulld`, `mullw`, `mulhw`, `mulhwu`, `mulhd`, `mulhdu`, `divd`, `divdu`, `divw`, `divwu`, `and`, `andc`, `nand`, `nor`, `orc`, `eqv`, `neg`, `or`, `xor`, `slw`, `srw`, `sld`, `srd`, `sraw`, `srawi`, `srad`, `cntlzw`, `cntlzd`, `popcntb`, `popcntw`, `popcntd`, `extsb`, `extsh`, `extsw`, `mfcr`, `mtcrf`, `mcrxr`), condition-register moves and logical operations (`mcrf`, `crand`, `crxor`, `crnor`, `creqv`, and related forms), selected single/double-precision FPU arithmetic and comparisons (`fadd[s]`, `fsub[s]`, `fmul[s]`, `fdiv[s]`, `fcmpu` for ordered and unordered values, `fcmpo` for ordered non-NaN values, `fabs`, `fnabs`, `fneg`, `fmr`, `frsp`, `fsel`, `frin`, `friz`, `frip`, `frim`, `fctiwz`), FPR loads/stores (`lfs[u][x]`, `lfd[u][x]`, `stfs[u][x]`, `stfd[u][x]`), LR/CTR SPR reads and writes (`mfspr`, `mtspr` restricted to XER (SPR 1), LR, and CTR), word 32-bit and 64-bit rotate/mask operations (`rlwinm`, `rlwimi`, `rlwnm`, `rldicl`, `rldicr`, `rldic`, `rldimi`), direct/conditional/indirect branches (`b`, `bc`, `bclr`, `bcctr`), and displacement/indexed byte/halfword/word/doubleword loads and stores, including selected update forms (`lwz[u][x]`, `stw[u][x]`, `lbz[u][x]`, `stb[u][x]`, `lhz[u][x]`, `lha[u][x]`, `sth[u][x]`, `ld[u][x]`, `std[u][x]`, `lwa[x]`, `lwbrx`, `lhbrx`, `ldbrx`, `stwbrx`, `sthbrx`, `stdbrx`). XER carry is modeled for the listed carry instructions. Divide-by-zero and signed 64-bit division overflow are rejected. Vector FPU operations are not implemented. The `sc` instruction returns a dedicated dispatcher signal; no PS3 syscall is implemented. Loads/stores use an explicitly bounded big-endian memory buffer; out-of-range, overflowing effective-address, and misaligned accesses fault without advancing the PC. The interpreter rejects invalid fetches and returns unsupported for all other opcodes. Compare instructions write the selected CR field using signed or unsigned 32/64-bit semantics. OE-enabled arithmetic and exceptions are not implemented. The harness uses synthetic instruction bytes, not a PS3 executable.

The same folder contains a bounded ELF64 PPC64 parser/segment mapper, an ISO9660 reader for locating `PS3_GAME/USRDIR/EBOOT.BIN`, and a parser for SELF plus its public ELF/segment metadata. The console host and GUI memory-map the user's ISO read-only and report EBOOT metadata without extracting it. Observed values: EBOOT offset 6,750,208 and size 5,465,224; SELF version 2, metadata offset 1,040, header size 2,432 and data size 5,462,792; ELF machine 21 (PPC64), entry `0x50ddc0`, eight program headers. Both nonempty program segments carry encryption marker 1. No encrypted certification or segment payload is loaded, no game PPU code runs, and no renderer is integrated. The 230 checks include synthetic ISO and SELF metadata cases; actual ISO observations are read-only diagnostics, not a boot test. Format reference: [PS3 SELF format](https://www.psdevwiki.com/ps3/SELF_-_SPRX).

Build and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Build-PpuPrototype.ps1
```

Recorded local result: the native x64 harness built with Zig 0.17.0, and 230 checks passed, including PPU/ELF cases plus ISO9660 EBOOT lookup and SELF/ELF metadata parsing. Those container checks use synthetic buffers; they are not game boot tests. Output is under ignored `work/`; it is not committed. A prior Zig 0.15.2 distribution was incomplete and was not used for the successful build.

## Unknowns / Incertidumbres

PPU ISA coverage and dynamic recompilation; a correct supported SELF loader; SPU instruction/DMAs/synchronization/scheduling; PS3 service requirements; RSX command buffers/shaders/resource semantics; audio/input; long-session memory use; AMD presentation/frame pacing; and actual frame timing. The user's ISO has only been memory-mapped read-only to inspect EBOOT and SELF/ELF metadata. No game bytes were changed, extracted, decrypted, copied, or executed. The ZIP remains untouched.

## Go/no-go / Decisión

**GO** for small asset-free runtime prototypes and synthetic validation. **NO-GO** for claiming a playable port or committing to a 50–60 FPS result. The immediate gate is a supported SELF loader and title-specific startup services, followed by broader PPU support, SPU, and RSX-to-Vulkan. The project may discover that 50–60 FPS is not attainable on this hardware.

**SEGUIR** con prototipos pequeños sin recursos del juego y validación sintética. **NO** afirmar que existe un port jugable ni prometer 50–60 FPS. La siguiente etapa es ampliar el core PPU y contar con un ejecutable descifrado adecuado. Después se implementan servicios, SPU y RSX a Vulkan de forma incremental. Es posible descubrir que esta PC no alcanza 50–60 FPS.

## External references / Referencias

- [OpenPOWER ISA specifications](https://openpowerfoundation.org/specifications/isa/)
- [Khronos Vulkan Registry](https://registry.khronos.org/vulkan/)
- [`gow3_pc`](https://github.com/FelipePrado0/gow3_pc) (PS4 project; design reference only)
