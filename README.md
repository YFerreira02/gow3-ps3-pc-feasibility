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
- Asset-free PPU interpreter prototype: 230 synthetic checks cover selected PPU operations, bounded ELF loading, ISO9660 EBOOT lookup, and bounded SELF/ELF metadata parsing. The SELF inspector does not decrypt, extract, or execute game data.
- A read-only scan of the user's ISO located `EBOOT.BIN` at byte offset 6,750,208 (5,465,224 bytes). Its SELF metadata exposes a PPC64 ELF entry at `0x50ddc0` and eight segment descriptors; payload encryption markers are set on the two nonempty program segments. This identifies the loader blocker; it does not reach PPU code or a menu.
- Game data and firmware are not in the repository. The ISO is only memory-mapped read-only by the diagnostic host; no second copy or extracted file is created.

## Next gates / Siguientes etapas

1. Grow a verified PPU decoder/interpreter and then a small x64 dynamic recompiler.
2. Implement and validate a suitable SELF loader and supported PS3 execution path. Current code reports container/ELF metadata only; encrypted certification and executable segments are not loaded.
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

## Build the standalone host executable

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Build-PpuHost.ps1
```

The executable is written to `work\PpuHost\Gow3PpuHost.exe`. It accepts a raw big-endian PPC64 ELF for the synthetic harness, or reads an ISO in read-only mode to locate EBOOT and report SELF/ELF metadata. It does not load SELF payloads or implement PS3 syscall services, and cannot launch *God of War III*.

El ejecutable queda en `work\PpuHost\Gow3PpuHost.exe`. Recibe un ELF PPC64 big-endian para el harness sintético, o inspecciona la ISO en modo de solo lectura para localizar EBOOT y mostrar la cabecera SELF. Aún no carga SELF ni implementa servicios PS3, así que todavía no inicia *God of War III*.

## GUI prototype / Prototipo con interfaz

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Build-PpuGui.ps1
```

This opens `work\PpuGui\Gow3PpuGui.exe`, a Win32 window with a file picker, instruction-budget field, and execution status. It can run a raw PPC64 ELF prototype or read-map an ISO to report EBOOT's SELF header. It still cannot load the encrypted game, reach its menu, or render the game.

Este comando abre `work\PpuGui\Gow3PpuGui.exe`, una ventana Win32 con selector de archivo, límite de instrucciones y estado. Puede probar un ELF PPC64 crudo o leer la ISO para mostrar los datos SELF/ELF de EBOOT. Aún no carga los segmentos del juego, no llega al menú y no renderiza el juego.

## Repository map

- `src/Gow3Runtime/ppu/`: initial asset-free PPU interpreter prototype.
- `src/Gow3Runtime/ppu/ppu_elf.c`, `ppu_iso.c`, `ppu_self.c`: bounded ELF mapper, read-only ISO9660 EBOOT locator, and bounded SELF/ELF metadata parser; no SELF decryption/segment loader.
- `tools/Build-PpuPrototype.ps1`: build/run command for the PPU prototype.
- `docs/approach.md`: standalone scope and gates; RPCS3 is excluded.
- `docs/architecture.md`: component architecture and technical limits.
- `docs/decision.md`, `docs/viability.md`: recorded measurements and risks.
- `tools/`: Vulkan 1.2 host probes; they contain no game workload.

## Referenced project and license

[`gow3_pc`](https://github.com/FelipePrado0/gow3_pc) is a PS4-targeted project and is used as a structural reference only. No code has been copied. Its repository declares GPL-2.0; any future code reuse requires a deliberate license review. This repository has not yet selected a license.

## Referencia y licencia

[`gow3_pc`](https://github.com/FelipePrado0/gow3_pc) está dirigido a PS4 y se usa solo como referencia estructural. No se ha copiado código. El repositorio declara GPL-2.0; cualquier reutilización futura requiere revisar la licencia. Este repositorio aún no selecciona una licencia.
