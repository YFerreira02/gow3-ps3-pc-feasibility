# Standalone runtime decision / Decisión del runtime independiente

**Date / Fecha:** 2026-10-19
**Target / Objetivo:** Windows 11 x64; Ryzen 5 3500U; Radeon Vega 8; 10 GB RAM; Vulkan 1.2 minimum.

## Decision / Decisión

Proceed with a standalone, game-specific PS3 runtime prototype. **RPCS3 is not part of the product path.** Its launcher and profile-writer sources have been removed from the current branch. The product is not playable and the 50–60 FPS goal is unverified.

Continuar un prototipo independiente de runtime PS3 específico para el juego. **RPCS3 no forma parte de la ruta del producto.** Sus fuentes antiguas del launcher y perfiles se quitaron de la rama actual. El producto no es jugable y la meta de 50–60 FPS no está verificada.

“Native” means a Windows program with its own user interface and renderer. It does not mean executing PS3 machine code directly. A standalone runtime still has to translate PPU/SPU instructions and implement PS3 services and RSX behavior. A Windows app and Vulkan 1.2 probe are not evidence that this can meet the target.

## Evidence so far / Evidencia actual

- Host hardware report: Ryzen 5 3500U (4 cores / 8 threads), Radeon Vega 8, about 10 GB RAM; the supplied Task Manager image showed 92% memory use at that moment.
- Prior project Vulkan probes reported Vulkan 1.2 on Vega 8 and rendered synthetic output. Offscreen throughput varied strongly with internal resolution. No game code/workload ran.
- New PPU foundation in `src/Gow3Runtime/ppu/`: a C reference interpreter for a small, explicitly incomplete subset of integer, branch, and memory instructions. A standalone Windows console host now loads raw PPC64 ELF files and runs to a bounded budget or syscall handoff; only synthetic input has been used. This is not a game-compatible CPU or launcher.
- The Windows x64 PPU prototype builds with Zig 0.17.0 and currently passes 228 synthetic checks. The separate console and GUI builds also succeed. The earlier Zig 0.15.2 package is incomplete and was not used for that result.
- Legacy RPCS3 launcher/profile-writer files were removed from the current source tree as part of the standalone pivot; they remain only in Git history.
- The user's ISO was memory-mapped read-only to locate EBOOT and parse its fixed SELF header. No game content was extracted, decrypted, executed, copied, or moved. No game file is in Git.

## Risks and gates / Riesgos y etapas

The largest unknowns are PPU coverage and dynamic recompilation, SPU instruction/scheduling compatibility, PS3 services, RSX command/shader translation, memory pressure, and stable presentation on this AMD driver. 50–60 FPS is only assessable after representative gameplay runs. Each gate may stop or rescope the effort; no performance promise is made.

Next: design and validate the SELF loader boundary, then add title-required PS3 startup services. Current code cannot enter game PPU code. Do not create duplicate game data.

La mayor incertidumbre está en cobertura/recompilación PPU, compatibilidad y planificación SPU, servicios PS3, traducción de comandos/shaders RSX, presión de memoria y presentación estable con este driver AMD. Los 50–60 FPS solo se pueden evaluar con juego representativo. Cada etapa puede detener o reducir el alcance; no se promete rendimiento.

Siguiente: ampliar la cobertura PPU con la ISA publicada y conseguir un ELF descifrado adecuado. Las pruebas dependientes del juego esperan un ejecutable compatible y servicios PS3 que aún faltan. No duplicar archivos del juego.

## References / Fuentes

- [OpenPOWER ISA specification](https://openpowerfoundation.org/specifications/isa/)
- [Khronos Vulkan Registry](https://registry.khronos.org/vulkan/)
- [`gow3_pc` repository](https://github.com/FelipePrado0/gow3_pc) (PS4-targeted design reference only)
