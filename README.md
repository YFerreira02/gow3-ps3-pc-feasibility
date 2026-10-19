# God of War III PS3 → PC — Feasibility Study

**Status:** feasibility research; no playable port is promised or implemented yet.
**Target OS:** Windows 11.
**Minimum graphics API:** Vulkan 1.2.
**Initial reference PC:** AMD Ryzen 5 3500U, Radeon Vega 8 integrated graphics, 10 GB RAM.
**Performance goal to investigate:** 50–60 FPS, with adjustable render resolution.

This repository studies whether a game-specific PC runtime for the **PlayStation 3 edition** of *God of War III* could be feasible on the reference PC. The design takes inspiration from the game-specific runtime, launcher, configuration, and patch workflow of [`gow3_pc`](https://github.com/FelipePrado0/gow3_pc), while recognizing that the PS3 edition requires a different execution and graphics path.

The project is a feasibility study with local Vulkan probes and a read-only metadata check of a local PS3 disc image; this check does not establish ownership or rights. The probes do not execute game code or extract game assets. They do not include game files or claim that a full port will reach the performance target.

## Key technical distinction

`gow3_pc` targets the PS4 remaster. Its original game code is x86-64 and its graphics path uses shadPS4's Vulkan renderer. The PS3 edition uses a different CPU/GPU architecture and system interface. Its executable, SPU workloads, RSX command stream, and PS3 services cannot be run by that PS4 runtime as-is. A PS3-to-PC effort would need its own game-specific translation/runtime work; Vulkan 1.2 is a graphics API floor, not a guarantee of compatibility or speed.

## Initial findings

- The ISO at `C:\Games\God_Of_War_3\God of War III (Europe) (En,Fr,De,Es,It,Nl,Pt,Pl,Ru).iso` is about 43.2 GB.
- A temporary read-only mount showed a UDF volume labelled `PS3VOLUME`, with `PS3_GAME`, `PS3_UPDATE`, and `PS3_DISC.SFB` at the root.
- `PS3_GAME\USRDIR\EBOOT.BIN` is about 5.5 MB and begins with an `SCE` container signature.
- The ISO was unmounted after inspection. No files were extracted or modified. Game files must stay outside this repository.

These observations confirm a PS3-disc-like structure, but do not yet identify the executable's decrypted format, enumerate its required system calls, or establish performance feasibility. The launcher also passes a user-owned ISO directly to RPCS3 without creating an extracted duplicate. Encrypted disc images may require a matching user-owned decryption key.

The local PC was also inspected read-only. Windows reports build 26300.9711 (Windows 11 26H2); the Ryzen 5 3500U has 4 cores / 8 threads and the system has about 9.9 GB RAM. `vulkaninfo` reports Vulkan 1.2.170 on both the Vega 8 and Radeon 530. The Vega 8 exposes the checked Vulkan 1.2 features recorded in the feasibility report. Project-owned probes then successfully created a Vulkan 1.2 graphics device, a Win32 surface, a swapchain, and presented a synthetic clear-color frame on Vega 8. This verifies a basic host rendering path, not game compatibility or speed. `vulkaninfo` emitted an ICD warning during enumeration; see the report for the caveat.

The ISO's `PARAM.SFO` identifies title ID `BCES00510`, Europe, application version `01.00`, and disc version `01.03`. These identifiers help constrain later compatibility research; no game payload is stored here.

## Preliminary decision

Proceed with a **focused feasibility prototype**, not a promise of a full port. The PC meets the stated Vulkan 1.2 minimum, but 50–60 FPS remains unverified and high risk on this low-power CPU/iGPU. A game-specific Windows application can have a native PC interface and Vulkan renderer, but running the PS3 build still requires CPU/system translation and RSX emulation/translation. See [`docs/architecture.md`](docs/architecture.md) and [`docs/viability.md`](docs/viability.md).

The Win32 swapchain path remains unstable on the target AMD driver: FIFO reaches three presents and then times out waiting for another image; immediate mode crashes. A separate Vulkan 1.2 offscreen path avoids the swapchain and copies a synthetic clear to Windows for display. It completed 60 validation-enabled frames at 640×360 internal resolution scaled to a 1280×720 window. Clear/readback throughput ranged from about 49–75 frames/s at that scale across runs, and fell to about 33 at 960×540 and 20 at 1280×720 internal. These are not game FPS measurements: the probe does not run PS3 code, PPU/SPU translation, RSX commands, or game shaders. See the [decision memo](docs/decision.md) and [feasibility report](docs/viability.md).

## Repository map

- [`docs/viability.md`](docs/viability.md): current scope, evidence, risks, and next investigation steps.
- [`docs/architecture.md`](docs/architecture.md): candidate runtime structure and technical decision points.
- [`docs/approach.md`](docs/approach.md): compares a custom PS3 runtime with RPCS3 integration and sets the next performance gates.
- [`docs/decision.md`](docs/decision.md): bilingual go/no-go findings and evidence gates.
- [`tools/Test-Vulkan12.ps1`](tools/Test-Vulkan12.ps1): dependency-free Windows check that creates a Vulkan 1.2 instance, enumerates adapters, and creates a graphics queue on the preferred adapter.
- [`tools/Test-VulkanWin32Surface.ps1`](tools/Test-VulkanWin32Surface.ps1): creates a hidden Win32 surface and checks presentation support on the preferred adapter.
- [`tools/Test-VulkanSwapchain.ps1`](tools/Test-VulkanSwapchain.ps1): creates a hidden Win32 surface and Vulkan swapchain, clears one image, and presents it.
- [`tools/Test-VulkanTriangle.ps1`](tools/Test-VulkanTriangle.ps1): creates a shader pipeline, draws a synthetic triangle, and accepts configurable swapchain dimensions.
- [`tools/VulkanTriangleProbe.cs`](tools/VulkanTriangleProbe.cs): Vulkan interop and pipeline implementation used by the triangle probe.
- [`tools/VulkanNativePresentProbe.c`](tools/VulkanNativePresentProbe.c): native C cross-check of Vulkan acquire/clear/present and swapchain teardown, used only for diagnosing the target AMD driver.
- [`tools/Build-VulkanNativePresentProbe.ps1`](tools/Build-VulkanNativePresentProbe.ps1) and [`tools/Run-VulkanNativePresentProbe.ps1`](tools/Run-VulkanNativePresentProbe.ps1): build and run the native cross-check with an installed Zig compiler and Vulkan SDK.
- [`tools/VulkanOffscreenPresentProbe.c`](tools/VulkanOffscreenPresentProbe.c): Vulkan 1.2 offscreen clear/readback path that avoids Win32 swapchains.
- [`tools/Measure-Process.ps1`](tools/Measure-Process.ps1): records process CPU/memory/thread metrics and available system RAM to an ignored CSV for baseline profiling; it does not report FPS.
- [`src/Gow3Launcher/`](src/Gow3Launcher/): Windows Forms front end for a user-installed RPCS3, with PS3 title-ID checks and a Vulkan 1.2 probe button.
- [`tools/Publish-Gow3Launcher.ps1`](tools/Publish-Gow3Launcher.ps1): publishes a self-contained Windows x64 launcher under ignored `work/`.
- [`tools/Build-VulkanOffscreenProbe.ps1`](tools/Build-VulkanOffscreenProbe.ps1) and [`tools/Run-VulkanOffscreenPresentProbe.ps1`](tools/Run-VulkanOffscreenPresentProbe.ps1): build the offscreen probe and display/scale its output through a Windows window; internal and output dimensions are independent.
- [`tools/Compile-Shaders.ps1`](tools/Compile-Shaders.ps1): rebuilds the checked-in SPIR-V files from the GLSL sources.
- `.gitignore`: excludes common disc images, extracted game folders, and game archives from Git.

## Launcher prototype / Prototipo del launcher

This Windows x64 launcher is a front end for an **external RPCS3 installation**, not a PS3 emulator or a native game port. It accepts an extracted `PS3_GAME` folder, checks for `USRDIR\EBOOT.BIN` and `PARAM.SFO` title ID `BCES00510`, can run the repository's Vulkan 1.2 probe, remembers paths locally, and creates a per-game RPCS3 profile. The profile selects Vulkan on the Vega 8, offers 25–100% render scale relative to 1280×720, and enables RPCS3's FPS/frame-time overlay for measurement. Existing custom profiles are backed up before replacement. It can pass a locally selected firmware file to RPCS3 --installfw and opens Sony’s official update page. A selected ISO is passed directly to RPCS3 without extraction or a second game copy; an encrypted image may need its matching user-provided disc key. The package does not bundle RPCS3, add game patches, or promise better performance. Use a game dump and firmware obtained lawfully for your jurisdiction.

Build and publish a self-contained Windows 11 x64 app:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Publish-Gow3Launcher.ps1
```

Run `work\Gow3LauncherRelease\GOW3 PS3 Launcher.exe`, select your own `rpcs3.exe`, then select the extracted PS3_GAME folder or choose Browse ISO to pass your own ISO directly to RPCS3 without extracting a duplicate. If the image is encrypted, RPCS3 may require its matching user-provided disc key. If firmware is missing, download it from Sony's official page and install it with the firmware button while RPCS3 is closed. Choose a render scale; 50% is approximately 640×360, 75% is 960×540, and 100% is 1280×720. Starting the game writes the title-specific profile and then starts RPCS3 with `--no-gui` and the selected game path; turn direct boot off to let RPCS3 show its main window. The first launch may build PPU/SPU caches. Vulkan is checked separately with the launcher button. The game is not included, and the scale has not yet been benchmarked in gameplay.

The publish script also creates `work\Gow3Launcher-win-x64.zip` for easy local transfer.

Este launcher x64 para Windows es una interfaz para una instalación **externa de RPCS3**, no un emulador PS3 ni un port nativo. Acepta una carpeta `PS3_GAME` extraída, verifica `USRDIR\EBOOT.BIN` y el ID de `PARAM.SFO` `BCES00510`, ejecuta el probe Vulkan 1.2, guarda las rutas y crea un perfil por juego. El perfil selecciona Vulkan en la Vega 8, ofrece escala interna de 25–100% respecto a 1280×720 y activa el overlay de FPS/tiempos de cuadro de RPCS3 para medir. Respalda el perfil anterior si existe. Permite seleccionar firmware local y pasarlo a RPCS3 con --installfw, y abre la página oficial de actualizaciones de Sony. Pasa una ISO directamente a RPCS3 sin extraerla ni crear otra copia; una imagen cifrada puede requerir una clave propia compatible. No incluye RPCS3, agrega parches del juego ni promete más rendimiento. Usa una copia del juego y firmware obtenidos legalmente en tu jurisdicción.

Compila y publica una app autocontenida para Windows 11 x64:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Publish-Gow3Launcher.ps1
```

Ejecuta `work\Gow3LauncherRelease\GOW3 PS3 Launcher.exe`, selecciona tu propio `rpcs3.exe` y luego la carpeta PS3_GAME extraída o pulsa Buscar ISO para pasar tu ISO directamente a RPCS3 sin crear otra copia. Si la imagen está cifrada, RPCS3 puede necesitar su clave propia correspondiente. Si falta firmware, descárgalo desde la página oficial de Sony e instálalo con el botón de firmware, con RPCS3 cerrado. Elige una escala: 50% equivale aproximadamente a 640×360, 75% a 960×540 y 100% a 1280×720. Al iniciar se guarda el perfil del título y RPCS3 recibe la ruta del juego; desactiva el inicio directo para mostrar su ventana principal. Vulkan se comprueba por separado. El juego no está incluido y la escala aún no se ha medido durante el juego.

El script de publicación también crea `work\Gow3Launcher-win-x64.zip` para transferirlo localmente.

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

La ruta de swapchain Win32 sigue inestable con el driver AMD del equipo: FIFO llega a tres presentaciones y luego vence el tiempo al esperar otra imagen; el modo inmediato cae. Una ruta Vulkan 1.2 fuera de pantalla evita el swapchain y copia un clear sintético a Windows para mostrarlo. Completó 60 frames con validación activa a 640×360 interno, escalados a una ventana 1280×720. El rendimiento de clear/copia osciló entre 49 y 75 frames/s en esa escala, y bajó a unos 33 a 960×540 y 20 a 1280×720 internos. No son mediciones del juego: el probe no ejecuta código PS3, traducción PPU/SPU, comandos RSX ni shaders del juego. Consulta el [memo de decisión](docs/decision.md) y el [informe de viabilidad](docs/viability.md).

### Diferencia técnica clave

`gow3_pc` está dirigido a la remasterización de PS4. El código original del juego es x86-64 y la ruta gráfica usa el renderer Vulkan de shadPS4. La versión de PS3 tiene otra arquitectura de CPU/GPU y otra interfaz de sistema. El ejecutable, las cargas de SPU, los comandos de RSX y los servicios de PS3 no funcionan directamente con ese runtime de PS4. Un esfuerzo para PS3 necesitaría su propia traducción/runtime; Vulkan 1.2 es un mínimo para la API gráfica, no una garantía de compatibilidad o rendimiento.

### Hallazgos iniciales

- La ISO ubicada en `C:\Games\God_Of_War_3\God of War III (Europe) (En,Fr,De,Es,It,Nl,Pt,Pl,Ru).iso` mide aproximadamente 43.2 GB.
- Al montarla temporalmente en modo de solo lectura se observó un volumen UDF llamado `PS3VOLUME`, con `PS3_GAME`, `PS3_UPDATE` y `PS3_DISC.SFB` en la raíz.
- `PS3_GAME\USRDIR\EBOOT.BIN` mide aproximadamente 5.5 MB y comienza con una firma de contenedor `SCE`.
- La ISO se desmontó después de inspeccionarla. No se extrajeron ni modificaron archivos. Los archivos del juego deben permanecer fuera de este repositorio.

La ruta gráfica pasó sus primeras puertas en la Vega 8: crea un pipeline Vulkan, dibuja un triángulo sintético y admite salida 960×540 y 1280×720. El swapchain Win32 se bloquea tras varias imágenes, así que también se probó la ruta fuera de pantalla con escalado por Windows. La carga PPU/SPU del juego y la meta de 50–60 FPS siguen sin probarse. Consulta el [memo de decisión](docs/decision.md).

Estas observaciones confirman una estructura similar a la de un disco de PS3, pero todavía no identifican el formato descifrado del ejecutable, sus llamadas de sistema requeridas ni la viabilidad del rendimiento. El launcher también pasa una ISO propia directamente a RPCS3 sin crear una copia extraída. Las imágenes cifradas pueden requerir una clave de descifrado propia.

### Licencia y reutilización

Todavía no se ha elegido una licencia para este proyecto. `gow3_pc` declara GPL-2.0. Antes de copiar o adaptar código hay que revisar las obligaciones de esa licencia y decidir si se acepta una licencia compatible. Hasta entonces, se usa solo como referencia de diseño.
