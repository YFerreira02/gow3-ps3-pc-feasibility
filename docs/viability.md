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

`src/Gow3Runtime/ppu/` now contains a small C reference interpreter and a Windows x64 harness. Supported instructions are `addi`, `addis`, `ori`, and unconditional `b` (relative/absolute addressing and link state); the interpreter rejects invalid fetches and returns unsupported for all other opcodes. The harness uses synthetic big-endian instruction bytes, not a PS3 executable.

Build and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Build-PpuPrototype.ps1
```

Recorded local result: the native x64 harness built with Zig 0.17.0, and eleven checks passed: arithmetic/immediate semantics, OR immediate, sign extension, bounds rejection, unsupported-opcode stop behavior, relative branch target/skip, link-register update, negative displacement, and absolute branch target. Output is under ignored `work/`; it is not committed. A prior Zig 0.15.2 distribution was incomplete and was not used for the successful build.

## Unknowns / Incertidumbres

PPU ISA coverage and dynamic recompilation; SPU instruction/DMAs/synchronization/scheduling; loader and PS3 service requirements; RSX command buffers/shaders/resource semantics; audio/input; long-session memory use; AMD presentation/frame pacing; and actual frame timing. There is no lawful decrypted game executable currently available in the project for these title-dependent analyses. The previously seen third-party ISO is not used, moved, copied, inspected, extracted, decrypted, or executed. The user's ZIP remains untouched.

## Go/no-go / Decisión

**GO** for small asset-free runtime prototypes and synthetic validation. **NO-GO** for claiming a playable port or committing to a 50–60 FPS result. The next acceptance gate is a broader ISA-tested PPU core; after that, design a loader only for a suitable lawful user-provided decrypted executable. Then add evidenced services, SPU, and RSX-to-Vulkan incrementally. The project may discover that 50–60 FPS is not attainable on this hardware.

**SEGUIR** con prototipos pequeños sin recursos del juego y validación sintética. **NO** afirmar que existe un port jugable ni prometer 50–60 FPS. La siguiente etapa es ampliar el core PPU probado contra la ISA; luego diseñar un loader para un ejecutable descifrado proporcionado legalmente por el usuario. Después se implementan servicios, SPU y RSX a Vulkan de forma incremental. Es posible descubrir que esta PC no alcanza 50–60 FPS.

## External references / Referencias

- [OpenPOWER ISA specifications](https://openpowerfoundation.org/specifications/isa/)
- [Khronos Vulkan Registry](https://registry.khronos.org/vulkan/)
- [`gow3_pc`](https://github.com/FelipePrado0/gow3_pc) (PS4 project; design reference only)
