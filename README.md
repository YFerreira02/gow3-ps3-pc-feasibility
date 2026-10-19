# God of War III PS3 → PC — standalone runtime research

**Status / Estado:** early prototype, not playable / prototipo temprano, aún no jugable.
**Target / Objetivo:** Windows 11 x64, Vulkan 1.2 minimum, Ryzen 5 3500U + Vega 8, 10 GB RAM.
**Goal / Meta:** investigate 50–60 FPS with adjustable resolution; not yet demonstrated.

## Direction / Dirección

This project aims to build a **standalone Windows app** for the PS3 edition of *God of War III*. **RPCS3 is excluded from the product and runtime path.** There is no RPCS3 launcher or dependency in the current source tree.

Este proyecto busca crear una **app independiente para Windows** para la versión de PS3 de *God of War III*. **RPCS3 queda fuera del producto y de la ruta de ejecución.** El árbol actual no contiene launcher ni dependencia para ese emulador.

“Native” here means a Windows program with its own host UI and renderer, not direct execution of PS3 machine code. The game still needs a PS3 compatibility runtime: PPU/SPU translation, PS3 services, and RSX-to-Vulkan translation. This is a major research effort. Vulkan 1.2 support does not guarantee that the game will run or reach 50–60 FPS. `gow3_pc` targets the PS4 remaster; it informs product organization only and cannot run the PS3 executable.

Aquí “nativo” significa un programa de Windows con interfaz y renderer propios; no significa ejecutar directamente las instrucciones de PS3. El juego aún requiere un runtime de compatibilidad: traducción PPU/SPU, servicios de PS3 y traducción RSX a Vulkan. Es un esfuerzo grande. Que exista Vulkan 1.2 no garantiza que el juego funcione ni alcance 50–60 FPS. `gow3_pc` apunta a la remasterización de PS4; sirve como referencia de organización, no puede ejecutar el binario de PS3.

## Implemented so far / Implementado hasta ahora

- Windows Vulkan probes: the Vega 8 exposes Vulkan 1.2; synthetic triangle and offscreen display paths have been exercised. These are not game benchmarks.
- Asset-free PPU interpreter prototype: `addi`, `addis`, `ori`/`oris`, `xori`/`xoris`, `andi.`/`andis.`, `cmpi`, `cmpli`, `b`, `bc`, `bclr`, `bcctr`, `lwz`, `stw`, `lbz`, and `stb`; it has bounded big-endian memory and virtual-address access. An ELF64 PPC64 parser maps synthetic segments, then the PPU executes synthetic instructions from the ELF entry address. This is not yet a game loader or usable emulator.
- No PS3 executable, ISO, firmware, decryption key, or game asset is in the repository.

## Next gates / Siguientes etapas

1. Grow a verified PPU decoder/interpreter and then a small x64 dynamic recompiler.
2. Design a safe loader for a lawful, user-provided decrypted ELF; the current third-party ISO is not used, copied, extracted, decrypted, or executed.
3. Discover the game’s required PS3 services and build only evidence-backed support.
4. Implement SPU translation/scheduling and RSX command processing to Vulkan 1.2.
5. Boot the title, measure representative gameplay on the target laptop, and only then assess 50–60 FPS.

Each stage can fail; do not market this as a playable port until it boots and sustained gameplay is measured. No duplicate game data is created. Game files stay outside Git.

Cada etapa puede fallar; no se presentará como port jugable hasta que inicie y se mida durante juego real. No se crea una copia adicional del juego. Los archivos permanecen fuera de Git.

## Build current PPU prototype

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Build-PpuPrototype.ps1
```

This uses the local Zig compiler under ignored `work/` to build a native Windows test executable. It runs synthetic tests only and never opens game files.

## Repository map

- `src/Gow3Runtime/ppu/`: initial asset-free PPU interpreter prototype.
- `src/Gow3Runtime/ppu/ppu_elf.c`: bounded ELF64 PPC64 parser/segment mapper; only synthetic buffers are used, with no SELF or decryption support.
- `tools/Build-PpuPrototype.ps1`: build/run command for the PPU prototype.
- `docs/approach.md`: standalone scope and gates; RPCS3 is excluded.
- `docs/architecture.md`: component architecture and technical limits.
- `docs/decision.md`, `docs/viability.md`: recorded measurements and risks.
- `tools/`: Vulkan 1.2 host probes; they contain no game workload.

## Referenced project and license

[`gow3_pc`](https://github.com/FelipePrado0/gow3_pc) is a PS4-targeted project and is used as a structural reference only. No code has been copied. Its repository declares GPL-2.0; any future code reuse requires a deliberate license review. This repository has not yet selected a license.

## Referencia y licencia

[`gow3_pc`](https://github.com/FelipePrado0/gow3_pc) está dirigido a PS4 y se usa solo como referencia estructural. No se ha copiado código. El repositorio declara GPL-2.0; cualquier reutilización futura requiere revisar la licencia. Este repositorio aún no selecciona una licencia.
