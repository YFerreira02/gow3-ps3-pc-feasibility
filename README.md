# God of War III PS3 → PC — Feasibility Study

**Status:** feasibility research; no playable port is promised or implemented yet.
**Target OS:** Windows 11.
**Minimum graphics API:** Vulkan 1.2.
**Initial reference PC:** AMD Ryzen 5 3500U, Radeon Vega 8 integrated graphics, 10 GB RAM.
**Performance goal to investigate:** 50–60 FPS, with adjustable render resolution.

This repository studies whether a game-specific PC runtime for the **PlayStation 3 edition** of *God of War III* could be feasible on the reference PC. The design takes inspiration from the game-specific runtime, launcher, configuration, and patch workflow of [`gow3_pc`](https://github.com/FelipePrado0/gow3_pc), while recognizing that the PS3 edition requires a different execution and graphics path.

The first phase is research and a small, read-only technical probe of a user-owned PS3 disc image. It does not execute game code, extract game assets, include game files, or claim that a full port will reach the performance target.

## Key technical distinction

`gow3_pc` targets the PS4 remaster. Its original game code is x86-64 and its graphics path uses shadPS4's Vulkan renderer. The PS3 edition uses a different CPU/GPU architecture and system interface. Its executable, SPU workloads, RSX command stream, and PS3 services cannot be run by that PS4 runtime as-is. A PS3-to-PC effort would need its own game-specific translation/runtime work; Vulkan 1.2 is a graphics API floor, not a guarantee of compatibility or speed.

## Initial findings

- The ISO at `C:\Games\God_Of_War_3\God of War III (Europe) (En,Fr,De,Es,It,Nl,Pt,Pl,Ru).iso` is about 43.2 GB.
- A temporary read-only mount showed a UDF volume labelled `PS3VOLUME`, with `PS3_GAME`, `PS3_UPDATE`, and `PS3_DISC.SFB` at the root.
- `PS3_GAME\USRDIR\EBOOT.BIN` is about 5.5 MB and begins with an `SCE` container signature.
- The ISO was unmounted after inspection. No files were extracted or modified. Game files must stay outside this repository.

These observations confirm a PS3-disc-like structure, but do not yet identify the executable's decrypted format, enumerate its required system calls, or establish performance feasibility.

The local PC was also inspected read-only. Windows reports build 26300.9711 (Windows 11 26H2); the Ryzen 5 3500U has 4 cores / 8 threads and the system has about 9.9 GB RAM. `vulkaninfo` reports Vulkan 1.2.170 on both the Vega 8 and Radeon 530. The Vega 8 exposes the checked Vulkan 1.2 features recorded in the feasibility report. This verifies the API floor on paper, not game compatibility or speed. The AMD Vulkan tool emitted an ICD warning during enumeration; see the report for the caveat.

The ISO's `PARAM.SFO` identifies title ID `BCES00510`, Europe, application version `01.00`, and disc version `01.03`. These identifiers help constrain later compatibility research; no game payload is stored here.

## Preliminary decision

Proceed with a **focused feasibility prototype**, not a promise of a full port. The PC meets the stated Vulkan 1.2 minimum, but 50–60 FPS remains unverified and high risk on this low-power CPU/iGPU. A game-specific Windows application can have a native PC interface and Vulkan renderer, but running the PS3 build still requires CPU/system translation and RSX emulation/translation. See [`docs/architecture.md`](docs/architecture.md) and [`docs/viability.md`](docs/viability.md).

## Repository map

- [`docs/viability.md`](docs/viability.md): current scope, evidence, risks, and next investigation steps.
- [`docs/architecture.md`](docs/architecture.md): candidate runtime structure and technical decision points.
- [`tools/Test-Vulkan12.ps1`](tools/Test-Vulkan12.ps1): dependency-free Windows check that creates a Vulkan 1.2 instance and enumerates adapters.
- `.gitignore`: excludes common disc images, extracted game folders, and game archives from Git.

## License and reuse

No project license is selected yet. `gow3_pc` declares GPL-2.0. Before copying or adapting its code, this project must review the applicable license obligations and decide whether a compatible license is acceptable. Until then, it is used as a design reference only.

## Estado (español)

**Fase:** estudio de viabilidad; todavía no existe un port jugable.
**Sistema objetivo:** Windows 11.
**API gráfica mínima:** Vulkan 1.2.
**PC de referencia:** AMD Ryzen 5 3500U, gráficos integrados Radeon Vega 8 y 10 GB de RAM.
**Meta de rendimiento a investigar:** 50–60 FPS, con resolución de renderizado ajustable.

Este repositorio estudia si sería viable crear un runtime para PC específico para la **versión de PlayStation 3** de *God of War III* en el PC de referencia. Toma como inspiración el runtime, el launcher, la configuración y el flujo de parches específicos para un juego de [`gow3_pc`](https://github.com/FelipePrado0/gow3_pc), pero reconoce que la edición de PS3 necesita una ruta de ejecución y gráficos distinta.

La primera fase consiste en investigar y hacer una prueba técnica pequeña, de solo lectura, sobre una imagen de disco propia. No ejecuta código del juego, no extrae recursos, no incluye archivos del juego y no promete que un port completo alcance la meta de rendimiento.

### Diferencia técnica clave

`gow3_pc` está dirigido a la remasterización de PS4. El código original del juego es x86-64 y la ruta gráfica usa el renderer Vulkan de shadPS4. La versión de PS3 tiene otra arquitectura de CPU/GPU y otra interfaz de sistema. El ejecutable, las cargas de SPU, los comandos de RSX y los servicios de PS3 no funcionan directamente con ese runtime de PS4. Un esfuerzo para PS3 necesitaría su propia traducción/runtime; Vulkan 1.2 es un mínimo para la API gráfica, no una garantía de compatibilidad o rendimiento.

### Hallazgos iniciales

- La ISO ubicada en `C:\Games\God_Of_War_3\God of War III (Europe) (En,Fr,De,Es,It,Nl,Pt,Pl,Ru).iso` mide aproximadamente 43.2 GB.
- Al montarla temporalmente en modo de solo lectura se observó un volumen UDF llamado `PS3VOLUME`, con `PS3_GAME`, `PS3_UPDATE` y `PS3_DISC.SFB` en la raíz.
- `PS3_GAME\USRDIR\EBOOT.BIN` mide aproximadamente 5.5 MB y comienza con una firma de contenedor `SCE`.
- La ISO se desmontó después de inspeccionarla. No se extrajeron ni modificaron archivos. Los archivos del juego deben permanecer fuera de este repositorio.

Estas observaciones confirman una estructura similar a la de un disco de PS3, pero todavía no identifican el formato descifrado del ejecutable, sus llamadas de sistema requeridas ni la viabilidad del rendimiento.

### Licencia y reutilización

Todavía no se ha elegido una licencia para este proyecto. `gow3_pc` declara GPL-2.0. Antes de copiar o adaptar código hay que revisar las obligaciones de esa licencia y decidir si se acepta una licencia compatible. Hasta entonces, se usa solo como referencia de diseño.
