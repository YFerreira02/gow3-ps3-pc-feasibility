# Standalone runtime approach / Enfoque independiente

## Product boundary / Límite del producto

The target is a Windows 11 application that starts independently and contains its own game-specific execution and Vulkan renderer path. RPCS3 is explicitly excluded as a dependency, subprocess, embedded component, benchmark requirement, and fallback. Its former launcher and profile-writer sources have been removed from the current tree. The project must not imply that a launcher alone makes the game native.

El objetivo es una aplicación Windows 11 que arranque por sí sola y tenga su propia ruta de ejecución específica y renderer Vulkan. RPCS3 queda expresamente excluido como dependencia, subproceso, componente integrado, requisito de benchmark y alternativa. Sus fuentes antiguas del launcher y perfiles se quitaron de la rama actual. No se debe sugerir que un launcher vuelve nativo al juego.

“Native PC application” describes the host program and output. PS3 instructions still require translation: Cell PPU and SPU machine code cannot run directly on x86-64. Required PS3 services, memory behavior, synchronization, and RSX command processing must also be implemented. This project will build a purpose-specific compatibility runtime; that is still emulation/translation work, just not RPCS3.

“App nativa de PC” describe el programa anfitrión y su salida. Las instrucciones PS3 aún requieren traducción: el código Cell PPU/SPU no corre directamente en x86-64. También hay que implementar los servicios PS3 necesarios, memoria, sincronización y procesamiento de comandos RSX. El proyecto construirá un runtime de compatibilidad específico; sigue siendo trabajo de emulación/traducción, solo que no usa RPCS3.

## Phased gates / Etapas

1. **PPU ISA foundation:** implement a decoder and reference interpreter for documented instructions, with strict bounds/alignment checks and synthetic tests. Current state: immediate and register signed/unsigned comparisons, selected D-form immediate operations, selected X-form integer arithmetic/logical/multiply/divide/carry/CR operations, selected double-precision FPU operations and comparisons, LR/CTR SPR access, word shifts/sign extensions, word rotate/mask instructions, direct/conditional/indirect branches, syscall handoff signaling, and displacement/indexed byte/halfword/word/doubleword memory operations with tested base updates. Single-precision/vector instructions, OE-enabled arithmetic, exceptions, and broad ISA coverage remain unsupported. Do not claim game compatibility. Encodings and semantics are checked against the [OpenPOWER ISA specification](https://openpowerfoundation.org/specifications/isa/).
2. **Native execution:** use profiling to decide whether to build an x86-64 dynamic recompiler for the verified instruction subset. Define invalidation, exceptions, ABI, and memory mapping before executing translated blocks.
3. **Container and ELF loading:** current milestone includes a PPC64 big-endian ELF parser and bounded PT_LOAD segment mapper validated on synthetic data; the PPU executes two instructions from the synthetic ELF entry. It does not read files from disk or support SELF/decryption. Actual game loading requires a lawful, user-provided decrypted executable. Do not inspect, copy, extract, decrypt, or execute the third-party ISO previously observed on this PC.
4. **PS3 services:** identify required imports/syscalls from a lawful executable and implement them one at a time with stubs and tests. No guessed offsets or title patches.
5. **SPU:** build instruction coverage, local store, DMA/mailbox, event, and scheduling semantics. The target CPU is low-power; interpreter-only SPU is unlikely to meet the performance goal.
6. **RSX to Vulkan 1.2:** decode command buffers and state, implement resources/render targets/synchronization, and translate shaders to SPIR-V. Treat the existing synthetic Vulkan probes only as host API checks.
7. **Game integration and performance:** boot representative scenes, record frametimes/CPU/GPU/memory/thermal behavior, and assess 50–60 FPS only after stable gameplay exists.

Every gate can reveal a blocker; don't skip ahead by presenting the UI as a working port. The target PC has Vega 8, Vulkan 1.2, Ryzen 5 3500U, and 10 GB RAM. Memory pressure is already material. Vulkan 1.2 is a compatibility floor, not a performance assurance.

## Reference project / Proyecto de referencia

[`gow3_pc`](https://github.com/FelipePrado0/gow3_pc) targets the PS4 remaster, whose host execution and graphics pipeline are different from the PS3 release. Borrow only the product ideas (game-specific launcher/configuration/patch separation) unless a future license review permits code reuse. The PS3 runtime here is a new implementation. The referenced repo declares GPL-2.0; no code has been copied, and this repository has not selected a license.

## Data and build boundary / Datos y compilación

The user's game files remain outside Git and are not duplicated. No project step should move or alter the ISO/ZIP without explicit instruction. Current PPU tests use synthetic 32-bit big-endian instruction encodings and cannot access game content.

Los archivos del juego permanecen fuera de Git y no se duplican. Ningún paso moverá o alterará ISO/ZIP sin instrucción expresa. Las pruebas PPU actuales usan instrucciones sintéticas de 32 bits en orden big-endian y no acceden al contenido del juego.

Primary references: [OpenPOWER ISA specification](https://openpowerfoundation.org/specifications/isa/) and [Khronos Vulkan Registry](https://registry.khronos.org/vulkan/). The PPU subset must be checked against the ISA edition/features used by PS3 PPE before expansion.
