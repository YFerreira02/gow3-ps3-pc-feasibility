# Runtime approach / Enfoque del runtime

## Decision / Decisión

**English.** Keep the Windows 11 product shell game-specific, but do not start a new PS3 CPU emulator. Evaluate a thin, licensed integration around RPCS3's mature PPU/SPU and RSX implementations, then add a God of War III profile, measurements, and narrowly verified workarounds. This is the only near-term route that reuses substantial PS3 compatibility work. It may still have RPCS3-like CPU cost; a game-specific launcher does not make the emulation workload cheaper by itself. A standalone runtime is a later option only if profiling identifies a concrete subsystem that can be replaced and benchmarked independently.

**Español.** Mantener específica para el juego la aplicación de Windows 11, pero no empezar desde cero otro emulador de CPU de PS3. Evaluar una integración delgada y compatible con las licencias alrededor de las implementaciones PPU/SPU y RSX maduras de RPCS3; después añadir el perfil de God of War III, mediciones y correcciones verificadas de forma puntual. Es la única ruta cercana que reutiliza una parte importante del trabajo de compatibilidad de PS3. Aun así, puede conservar un costo de CPU parecido al de RPCS3: un launcher específico no abarata por sí solo la emulación. Un runtime independiente queda para más adelante, y solo si el perfil de rendimiento identifica un subsistema concreto que pueda sustituirse y medirse por separado.

## Why the PS4 reference is not a PS3 base / Por qué la referencia PS4 no sirve como base PS3

The reference project's README says the PS4 executable runs directly on x86-64 and its runtime replaces PS4 system libraries; shadPS4 translates the PS4 GPU path to Vulkan. The PS3 release instead needs Cell PPU/SPU execution, PS3 services, and RSX translation. Its launcher, settings, patch organization, and profiling workflow are useful product references, but its PS4 execution layer is not reusable for the PS3 executable. See the [reference README](https://github.com/FelipePrado0/gow3_pc#readme).

El README del proyecto de referencia explica que el ejecutable de PS4 corre directamente en x86-64 y que su runtime reemplaza bibliotecas del sistema PS4; shadPS4 traduce la ruta gráfica de PS4 a Vulkan. La edición PS3 necesita ejecutar Cell PPU/SPU, ofrecer servicios de PS3 y traducir RSX. Su launcher, configuración, organización de parches y flujo de medición sí sirven como referencia de producto, pero la capa de ejecución PS4 no puede ejecutar el EBOOT de PS3. Consulta el [README de referencia](https://github.com/FelipePrado0/gow3_pc#readme).

## Options / Opciones

| Option | Benefit | Main risk | Decision |
| --- | --- | --- | --- |
| Reimplement PPU, SPU, PS3 services, and RSX | Full control over a narrow game profile | Recreates years of emulator work; likely worse compatibility and performance at first | Do not start here |
| Wrap/extend RPCS3 behind a game-specific Windows shell | Reuses its CPU, system, and Vulkan RSX work; faster path to a launchable experiment | GPL-2.0-only obligations for most RPCS3 files; internal APIs and build system are not a stable plug-in interface; performance may not improve | Investigate this first |
| Keep only the launcher/profile and use RPCS3 as an external dependency | Smallest integration and easiest licensing boundary for this repository | Does not reduce emulator resource use; product remains dependent on RPCS3 | Best first product prototype if the user accepts the tradeoff |

| Opción | Ventaja | Riesgo principal | Decisión |
| --- | --- | --- | --- |
| Reimplementar PPU, SPU, servicios PS3 y RSX | Control completo de un perfil limitado al juego | Repite años de trabajo de emulación; al principio probablemente tendrá peor compatibilidad y rendimiento | No empezar por aquí |
| Integrar/ampliar RPCS3 bajo una interfaz específica de Windows | Reutiliza CPU, sistema y renderer Vulkan RSX; ruta más rápida a un experimento ejecutable | La mayoría del código RPCS3 tiene GPL-2.0-only; sus APIs internas y compilación no son una interfaz estable de plugins; el rendimiento puede no mejorar | Investigar primero |
| Mantener solo launcher/perfil y usar RPCS3 como dependencia externa | Integración más pequeña y límite de licencias más sencillo para este repo | No reduce el consumo del emulador; el producto depende de RPCS3 | Mejor primer prototipo de producto si se acepta esa limitación |

RPCS3's repository identifies most of its files as GPL-2.0-only, with some files under other licenses. A private repository does not remove the need to respect those terms, especially before distributing binaries or source. For now, this project does not copy RPCS3 or `gow3_pc` code; it records links and evaluates integration boundaries. See [RPCS3's repository and license notice](https://github.com/RPCS3/rpcs3).

El repositorio de RPCS3 indica que la mayoría de sus archivos son GPL-2.0-only, aunque algunos tienen otras licencias. Que nuestro repositorio sea privado no elimina la obligación de respetar esas condiciones, sobre todo antes de distribuir código o binarios. Por ahora este proyecto no copia código de RPCS3 ni de `gow3_pc`; documenta enlaces y evalúa los límites de integración. Consulta el [repositorio y aviso de licencia de RPCS3](https://github.com/RPCS3/rpcs3).

## Performance gate / Puerta de rendimiento

The 50–60 FPS goal is **not an acceptance criterion until measured in the actual game**. Before building a custom execution layer, capture repeatable RPCS3 frame-time, CPU-thread, GPU, memory, and temperature data on the reference laptop with a legally obtained dump and known settings. Compare equivalent scenes at internal resolutions the Vega 8 can sustain. If the bottleneck is an SPU/PPU path, a graphics-only rewrite will not solve it. If it is RSX translation or readback, a targeted renderer experiment may be justified. Do not interpret Vulkan 1.2 support as evidence that the target FPS is achievable.

La meta de 50–60 FPS **no es un criterio de aceptación hasta medirla en el juego real**. Antes de construir una capa de ejecución propia, hay que recoger mediciones repetibles de tiempos de cuadro, hilos de CPU, GPU, memoria y temperatura en el portátil, con una copia obtenida legalmente y ajustes conocidos de RPCS3. Comparar escenas equivalentes y resoluciones internas que la Vega 8 pueda sostener. Si el límite es PPU/SPU, reescribir solo los gráficos no lo solucionará. Si el límite está en RSX o en las transferencias, podría justificarse un experimento específico del renderer. Tener Vulkan 1.2 no demuestra que se pueda alcanzar la tasa objetivo.

## Next gates / Siguientes etapas

1. Record the integration boundary and license review before importing any external source.
2. **Done:** build an asset-free Windows launcher/profile prototype. It locates user-selected RPCS3, checks an extracted PS3 EBOOT or passes a user-owned ISO directly to RPCS3 without extracting a duplicate, and offers a separate Vulkan 1.2 device probe and a local firmware installer using RPCS3's --installfw option; it remains an RPCS3 front end, not a native PS3 runtime.
3. Define a reproducible RPCS3 baseline measurement plan for a lawful user-provided dump. Keep all game data, firmware, keys, and logs containing proprietary content out of Git.
4. Investigate a small PPU/SPU/RSX technical experiment only after the baseline reveals a specific bottleneck and a build/toolchain route.
5. Reassess the standalone-port idea after data. Stop or rescope if the reference PC cannot meet the performance goal under the reused execution path.

The prototype source is `src/Gow3Launcher/`. To build the self-contained Windows x64 package, run `tools/Publish-Gow3Launcher.ps1`; output goes under ignored `work/Gow3LauncherRelease/`. The launcher creates a `config_BCES00510.yml` custom profile, selects Vulkan on the Vega 8, sets a user-selectable 25–100% resolution scale (based on 1280×720), and enables RPCS3's FPS/frame-time overlay. It backs up an existing profile before changing it. The selected resolution and frame-time behavior have not been measured in game. RPCS3 and game files are deliberately not bundled.

For a CPU/memory sample, start the chosen application and workload manually, then run `tools/Measure-Process.ps1 -ProcessName rpcs3 -DurationSeconds 180 -SampleIntervalSeconds 1`. The CSV defaults to the ignored `work/` folder. This recorder measures process CPU time, working set, private memory, thread count, handles, and system-available RAM. It does not measure FPS, GPU utilization, or game correctness; pair it with a frame-time capture and note the scene/settings separately. Use only a game dump obtained lawfully.

1. Registrar los límites de integración y revisar las licencias antes de importar código externo.
2. **Completado:** crear un prototipo de launcher Windows sin recursos del juego. Encuentra RPCS3 seleccionado por el usuario, comprueba un EBOOT extraído de PS3 o pasa una ISO propia directamente a RPCS3 sin extraer otra copia, y ofrece un probe independiente de Vulkan 1.2 e instalación local de firmware mediante --installfw de RPCS3; sigue siendo una interfaz para RPCS3, no un runtime PS3 nativo.
3. Definir mediciones repetibles de RPCS3 con una copia del usuario obtenida legalmente. Mantener fuera de Git los datos del juego, firmware, claves y registros que contengan contenido propietario.
4. Investigar un experimento pequeño de PPU/SPU/RSX solo cuando la medición inicial muestre un cuello de botella concreto y exista una ruta de compilación.
5. Revaluar el port independiente con datos. Detener o cambiar el alcance si el PC de referencia no alcanza la meta con la ruta que reutiliza el emulador.

El código está en `src/Gow3Launcher/`. Para crear el paquete autocontenido Windows x64, ejecuta `tools/Publish-Gow3Launcher.ps1`; el resultado queda en `work/Gow3LauncherRelease/`, excluida de Git. El launcher crea `config_BCES00510.yml`, selecciona Vulkan en la Vega 8, configura una escala interna ajustable del 25–100% con base en 1280×720 y activa el overlay de FPS/frame time de RPCS3. Respalda el perfil existente antes de cambiarlo. La escala y el frame pacing aún no se han medido en el juego. RPCS3 y los archivos del juego no se incluyen.

Para recoger una muestra de CPU/memoria, inicia manualmente la aplicación y la escena que se vaya a medir; luego ejecuta `tools/Measure-Process.ps1 -ProcessName rpcs3 -DurationSeconds 180 -SampleIntervalSeconds 1`. El CSV se guarda por defecto en `work/`, excluido de Git. El capturador registra tiempo de CPU del proceso, memoria física y privada, hilos, handles y RAM disponible del sistema. No mide FPS, uso de GPU ni funcionamiento correcto del juego; debe acompañarse de captura de tiempos de cuadro y anotaciones de escena/ajustes. Usa solo una copia del juego obtenida legalmente.

## Current dump handling / Manejo actual de la imagen

The adjacent `README.TXT` attributes the ISO to a ROM download site. This project will not inspect, extract, decrypt, execute, or publish that image. Future game-dependent tests require a dump the user has obtained from their own disc using a lawful method in their jurisdiction. The ISO and extracted game files remain excluded from Git.

El `README.TXT` junto a la ISO atribuye su origen a un sitio de descarga de ROMs. Este proyecto no inspeccionará, extraerá, descifrará, ejecutará ni publicará esa imagen. Las pruebas que dependan del juego requerirán una copia obtenida del disco propio mediante un método legal en la jurisdicción del usuario. La ISO y los archivos extraídos seguirán excluidos de Git.
