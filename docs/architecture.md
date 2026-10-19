# Candidate architecture / Arquitectura candidata

## Decision in one paragraph / Decisión resumida

The project should be a Windows 11 application with a game-specific launcher/configuration layer and a Vulkan renderer, inspired by the *shape* of `gow3_pc`. It cannot directly run the PS3 executable as ordinary x86-64 PC code. To use the PS3 release, it needs a translation/emulation layer for the PS3 CPU and system interfaces plus an RSX-to-Vulkan graphics path. Calling the app “native” can describe its Windows UI, build, and output renderer; it cannot truthfully mean unmodified PS3 machine code executes natively on the Ryzen CPU. This approach may still be more focused than a general-purpose console emulator, but focus alone does not guarantee higher FPS.

El proyecto debería ser una aplicación de Windows 11 con launcher/configuración específica del juego y un renderer Vulkan, inspirada en la *estructura* de `gow3_pc`. No puede ejecutar el EBOOT de PS3 como código x86-64 ordinario. Para usar la versión de PS3 necesita traducción/emulación de CPU e interfaces del sistema, además de una ruta gráfica RSX a Vulkan. “Nativo” puede describir la interfaz de Windows, la compilación y el renderer; no significa que el código PS3 sin modificar se ejecute directamente en el Ryzen. Este enfoque podría ser más específico que un emulador general, pero eso no garantiza más FPS.

## Proposed layers / Capas propuestas

| Layer | Responsibility | Feasibility notes |
| --- | --- | --- |
| Launcher and settings | Detect a user-provided game directory/dump, select adapter, choose output and internal resolution, logs, patches, controller settings. | Closest to the useful product structure of `gow3_pc`; relatively low risk. Never download or bundle the game. |
| Game profile | Known title ID/region/version, boot sequence, validated compatibility patches, game-specific workarounds. | Requires separate validation for `BCES00510` and version 1.03. Do not apply patches by guessed offsets. |
| PPU execution | Translate PowerPC PPU code to host x86-64, implement required PS3 system calls and thread/memory behavior. | Could use mature emulator components under their licenses or be reimplemented; a purpose-built translator is a major project. |
| SPU execution | Translate Cell SPU programs, scheduling, local stores, DMA/mailboxes, synchronization. | Likely a key CPU bottleneck. RPCS3 has an SPU LLVM JIT; a simple interpreter or per-game hacks risk severe performance/compatibility costs. |
| PS3 service layer | Filesystem, timers, events, audio, controller, save data, shaders, system modules needed by the title. | Must be driven by evidence from the game's own execution and logs; broad system compatibility is not required if the title profile is narrow. |
| RSX frontend | Consume PS3 graphics command buffers and GPU state; translate shaders, render targets, synchronization, and draw calls. | The most substantial graphics-specific work. Vulkan is the host API; it does not parse/translate RSX commands by itself. |
| Vulkan backend | Vulkan 1.2 device/swapchain, resource allocation, shader pipeline, presentation, resolution scaling. | Start with a synthetic scene and Vega 8. Avoid mandatory 1.3 features; check limits/extensions and driver behavior at runtime. |
| Diagnostics | Frame-time/CPU-GPU timing, translation cache, memory use, shader cache, compatibility log, crash dumps without game payload. | Essential to determine if a targeted runtime actually improves the user's experience over RPCS3. |

## How `gow3_pc` informs this project / Qué aporta `gow3_pc`

Useful design ideas to evaluate: a small loader, game-specific runtime boundary, configuration/patch layer, and clear separation between title logic and graphics backend. Its PS4 execution path is not a PS3 implementation: the PS4 remaster has x86-64 code, while the PS3 release has Cell PPU/SPU code and RSX commands. Its current shadPS4 path also documents a Vulkan 1.3 requirement, whereas this project must keep 1.2 as its minimum. Do not copy code until the project's GPL-2.0 obligations and compatibility are explicitly decided.

Ideas útiles: loader pequeño, límite claro del runtime específico del juego, configuración/parches separados y separación entre el perfil del juego y el backend gráfico. Su ruta de ejecución de PS4 no implementa PS3: la remasterización de PS4 usa código x86-64 y la edición de PS3 usa Cell PPU/SPU y comandos RSX. Además, la ruta actual de shadPS4 documenta Vulkan 1.3 como requisito, mientras este proyecto debe conservar 1.2 como mínimo. No copiar código hasta decidir expresamente las obligaciones de GPL-2.0.

## Candidate technical routes / Rutas técnicas candidatas

### A. Build all translation/runtime components from scratch

Maximum control over a game-specific design, but the riskiest and slowest option. It requires compatible PPU/SPU translation, PS3 service behavior, RSX command handling, shader conversion, audio, input, timing, and enough game-specific debugging to pass representative scenes. This route is not justified before a small prototype proves a concrete advantage.

### B. Reuse mature RPCS3 components selectively

Potentially reduces CPU/RSX implementation work, but creates licensing, integration, architecture, and maintenance obligations. The existing emulator's generalized compatibility can carry code paths that a single-title runtime would not need; pruning them safely is not automatic. The repo should first map component boundaries and license terms, then estimate whether a minimal host is actually smaller/faster.

### C. Keep RPCS3 as the baseline and build an external game profile/tooling

Lowest engineering risk and best comparison baseline: automate configuration documentation, title-specific patch discovery, and performance capture around RPCS3. It would not meet the user's “without RPCS3” end goal, but could show whether the current bottleneck is CPU, GPU, memory, or configuration before committing to a new runtime.

**Current recommendation:** Prototype the Vulkan 1.2 backend and a non-game synthetic workload first; investigate RPCS3 component reuse in parallel through source/license review. Do not start a full RSX or SPU implementation until these gates are passed.

## Performance gates / Puertas de rendimiento

The target requires an average frame budget of 16.7–20 ms at 50–60 FPS, with stable frame pacing. The full game can be limited by CPU translation, SPU scheduling, graphics translation, memory bandwidth, shader compilation, or game logic. Lower internal resolution only helps when GPU fill/render cost is the bottleneck; it will not fix CPU/SPU saturation. The Vega 8 shares system memory and the machine has about 10 GB total, so renderer allocations and cache growth need explicit budgets.

1. **Passed:** Vulkan 1.2 instance, physical device, logical graphics device/queue, Win32 surface, and queue presentation support on Vega 8.
2. **Next:** create a `VK_KHR_swapchain` and present a synthetic frame on Vega 8; resolve the `amdvlk64.dll` enumeration warning if it affects this path.
3. Measure a synthetic workload at several internal/output resolutions and log CPU/GPU frame times, memory use, and frame pacing.
4. Establish a lawful, reproducible way to profile the user's own game dump without putting it in GitHub.
5. Measure representative title workloads with a proven runtime as a reference; do not treat cutscenes or menus as gameplay performance.
6. Only claim feasibility for 50–60 FPS if sustained representative scenes meet the budget after shader warm-up and without large recurring stutters.

## Data boundary / Límite de datos

Keep the ISO, extracted files, EBOOT, firmware, keys, game shaders/assets, and title-specific copyrighted data out of this repository. Store only source code written for the project, public technical references, reproducible test descriptions, and non-sensitive measurements. User-supplied files remain on the user's machine.

## Sources / Fuentes

- [`gow3_pc` README and license](https://github.com/FelipePrado0/gow3_pc)
- [shadPS4 user requirements](https://github.com/shadps4-emu/shadPS4/wiki/I.-Quick-start-%5BUsers%5D/8eb552946733b22cdd78fddd9fd6eb197072e088)
- [RPCS3 SPU LLVM recompiler](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/SPULLVMRecompiler.cpp)
- [RPCS3 SELF container parser](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Crypto/unself.h)
- [RPCS3 developer information](https://github.com/RPCS3/rpcs3/wiki/developer-information)
- [Khronos Win32 surface extension](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_win32_surface.html)
