# Standalone runtime decision / Decisión del runtime independiente

**Date / Fecha:** 2026-10-19
**Target / Objetivo:** Windows 11 x64; Ryzen 5 3500U; Radeon Vega 8; 10 GB RAM; Vulkan 1.2 minimum.

## Decision / Decisión

Proceed with a standalone, game-specific PS3 runtime prototype. **RPCS3 is not part of the product path.** Do not use the old launcher to boot the game. The product is not playable and the 50–60 FPS goal is unverified.

Continuar un prototipo independiente de runtime PS3 específico para el juego. **RPCS3 no forma parte de la ruta del producto.** No usar el launcher anterior para arrancar el juego. El producto no es jugable y la meta de 50–60 FPS no está verificada.

“Native” means a Windows program with its own user interface and renderer. It does not mean executing PS3 machine code directly. A standalone runtime still has to translate PPU/SPU instructions and implement PS3 services and RSX behavior. A Windows app and Vulkan 1.2 probe are not evidence that this can meet the target.

## Evidence so far / Evidencia actual

- Host hardware report: Ryzen 5 3500U (4 cores / 8 threads), Radeon Vega 8, about 10 GB RAM; the supplied Task Manager image showed 92% memory use at that moment.
- Prior project Vulkan probes reported Vulkan 1.2 on Vega 8 and rendered synthetic output. Offscreen throughput varied strongly with internal resolution. No game code/workload ran.
- New PPU foundation in `src/Gow3Runtime/ppu/`: a C reference interpreter with only `addi`, `addis`, and `ori`; it uses synthetic instruction bytes, rejects misalignment/out-of-range fetches, and stops on unsupported instructions. This is not a game-compatible CPU.
- The local Zig 0.15.2 compiler package is incomplete: it fails due to a missing `libc/mingw/math/coshl.c`. A Zig 0.17.0 build attempt did not complete within the initial wait; no successful native artifact or test result is claimed yet.
- The previous launcher was built around RPCS3. It is now explicitly superseded and excluded from the product.
- The previously observed ISO was attributed by its adjacent README to a third-party ROM download source. The project will not inspect, extract, decrypt, execute, copy, or move that ISO. No game file is in Git.

## Risks and gates / Riesgos y etapas

The largest unknowns are PPU coverage and dynamic recompilation, SPU instruction/scheduling compatibility, PS3 services, RSX command/shader translation, memory pressure, and stable presentation on this AMD driver. 50–60 FPS is only assessable after representative gameplay runs. Each gate may stop or rescope the effort; no performance promise is made.

Next: repair/use a complete compiler toolchain; build and run the synthetic PPU checks; then expand instruction coverage against the published ISA. Next design a loader only for a lawful, user-provided decrypted executable. Game-dependent testing is blocked until suitable lawful inputs are available. Do not create duplicate game data.

La mayor incertidumbre está en cobertura/recompilación PPU, compatibilidad y planificación SPU, servicios PS3, traducción de comandos/shaders RSX, presión de memoria y presentación estable con este driver AMD. Los 50–60 FPS solo se pueden evaluar con juego representativo. Cada etapa puede detener o reducir el alcance; no se promete rendimiento.

Siguiente: reparar o usar una toolchain completa; compilar y ejecutar las comprobaciones sintéticas PPU; ampliar la cobertura con la ISA publicada. Después diseñar un loader únicamente para un ejecutable descifrado que el usuario haya obtenido legalmente. Las pruebas dependientes del juego esperan insumos adecuados. No duplicar archivos del juego.

## References / Fuentes

- [OpenPOWER ISA specification](https://openpowerfoundation.org/specifications/isa/)
- [Khronos Vulkan Registry](https://registry.khronos.org/vulkan/)
- [`gow3_pc` repository](https://github.com/FelipePrado0/gow3_pc) (PS4-targeted design reference only)
