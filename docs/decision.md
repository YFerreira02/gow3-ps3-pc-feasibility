# Feasibility decision / Decisión de viabilidad

**Date / Fecha:** 2026-10-19
**Target / Equipo objetivo:** Windows 11, Ryzen 5 3500U (4C/8T), Radeon Vega 8, ~9.9 GB RAM
**Game dump / Juego:** PS3 Europe `BCES00510`, disc version `01.03`
**Minimum graphics API / API gráfica mínima:** Vulkan 1.2

## Decision / Decisión

### English

- **GO:** continue a narrow PC graphics/runtime prototype. On the reference PC, the Vulkan 1.2 path creates an instance/device/graphics queue, creates a Win32 surface and FIFO swapchain, builds a basic vertex/fragment graphics pipeline, draws one synthetic triangle, and presents it at both 960×540 and 1280×720. This is only a single-frame smoke test.
- **NO-GO for a full game port commitment today:** the 50–60 FPS target is not demonstrated. No PS3 game code has been run, no PPU/SPU translation has been benchmarked, and no RSX commands/shaders from the game have been translated. A triangle proves only that a simple host Vulkan pipeline works.
- **Risk: high.** The PS3 build requires game-specific or reused PPU and SPU translation, PS3 services, and an RSX renderer. A standalone Windows app would still be translating/emulating those PS3 interfaces; Vulkan 1.2 does not remove that CPU work. `gow3_pc` is a useful product-structure reference, but it targets the PS4 remaster and its current requirements specify Vulkan 1.3.
- **Additional target-PC risks:** the supplied Task Manager snapshot showed 92% system memory use. The Vulkan Win32 swapchain probe reaches three FIFO presents with triple buffering and then times out acquiring the next image; immediate mode crashes in `amdvlk64.dll` (`0xC0000005`). Khronos-style image semaphores, per-image command buffers, a message pump, and synchronous rendering do not resolve the WSI stall. A swapchain-free offscreen Vulkan 1.2 probe with host readback and Windows scaling completed 60 validation-enabled frames. The synthetic clear/readback path measured about 49–75 frames/s at 640×360 internal / 1280×720 output, about 33 at 960×540, and about 20 at 1280×720 internal. These are not game FPS figures; the tests do not run PPU/SPU code, RSX workloads, or representative shaders.
- **The 50–60 FPS goal is unverified, not disproved.** The Ryzen 5 3500U/Vega 8 can present the test pipeline, but this says nothing quantitative about the game. Lowering output resolution helps only if GPU rendering is a bottleneck; it will not fix PPU/SPU CPU saturation.
- **A Windows launcher prototype now builds and starts.** It saves emulator/game paths locally, checks the PS3 `PARAM.SFO` title ID (`BCES00510`), offers the Vulkan 1.2 device probe, and can invoke an external RPCS3 with the extracted EBOOT. It does not bundle RPCS3 or game files, configure performance patches, or remove RPCS3's CPU cost. The exact game-launch path remains unverified because RPCS3 is not installed in the checked locations and no lawful extracted game folder is available for this test.

**Recommendation:** do not write a PPU/SPU emulator from scratch. First evaluate a game-specific Windows shell around RPCS3, with its CPU/system/RSX work kept as an external dependency or integrated only after a GPL-2.0-only review. This is the fastest path to a runnable experiment, but may retain RPCS3's resource cost and cannot be presented as an FPS fix. The offscreen Vulkan probe remains a graphics fallback experiment, not the proposed PS3 execution engine. Measure repeatable performance on the reference PC before deciding whether any custom subsystem can address a demonstrated bottleneck. See the [runtime approach](approach.md). The go/no-go for a playable game-specific runtime must wait for representative game-workload evidence.

### Español

- **SEGUIR:** continuar un prototipo pequeño del runtime/render de PC. En el equipo de referencia, Vulkan 1.2 ya crea instancia, dispositivo y cola gráfica; crea una superficie Win32 y un swapchain FIFO; construye un pipeline básico de vertex/fragment shaders; dibuja un triángulo sintético y lo presenta en una sola imagen a 960×540 y 1280×720. Es una prueba de un frame.
- **NO comprometerse todavía con un port completo:** no se ha demostrado la meta de 50–60 FPS. No se ha ejecutado código PS3, medido traducción PPU/SPU ni traducido comandos/shaders RSX del juego. El triángulo solo comprueba que funciona un pipeline Vulkan sencillo en el PC.
- **Riesgo alto.** La versión de PS3 necesita traducción PPU y SPU específica o reutilizada, servicios de PS3 y un renderer RSX. Una aplicación independiente de Windows todavía tendría que traducir/emular esas interfaces de PS3; Vulkan 1.2 no elimina ese trabajo de CPU. `gow3_pc` sirve como referencia de estructura del producto, pero apunta a la remasterización de PS4 y sus requisitos actuales indican Vulkan 1.3.
- **Riesgos adicionales del PC objetivo:** la captura del Administrador de tareas mostraba 92% de memoria del sistema. La prueba de swapchain Vulkan Win32 llega a tres presentaciones FIFO con triple buffer y luego vence el tiempo al pedir otra imagen; el modo inmediato cae en `amdvlk64.dll` (`0xC0000005`). Usar semáforos al estilo Khronos, buffers por imagen, un ciclo de mensajes y renderizado síncrono no resuelve el bloqueo WSI. Un probe Vulkan 1.2 fuera de pantalla, con copia a memoria del sistema y escalado de Windows, completó 60 frames con validación activa. La escena sintética midió aproximadamente 49–75 FPS a 640×360 interno y salida 1280×720, 33 a 960×540 y 20 a 1280×720 interno. No son cifras del juego: no ejecutan PPU/SPU, comandos RSX ni shaders representativos.
- **La meta de 50–60 FPS no está verificada, pero tampoco refutada.** Ryzen 5 3500U/Vega 8 puede presentar el pipeline sintético, pero eso no cuantifica el rendimiento del juego. Reducir la resolución de salida solo ayuda si el cuello de botella es la GPU; no resuelve saturación de CPU por traducción PPU/SPU.
- **El prototipo de launcher Windows ya compila y abre.** Guarda localmente las rutas del emulador y del juego, comprueba el ID de `PARAM.SFO` (`BCES00510`), ofrece el probe Vulkan 1.2 y puede invocar una instalación externa de RPCS3 con el EBOOT extraído. No incluye RPCS3 ni archivos del juego, no configura parches de rendimiento ni elimina el costo de CPU del emulador. El inicio del juego no se ha comprobado porque RPCS3 no está instalado en las ubicaciones revisadas y no hay una carpeta extraída obtenida legalmente para esta prueba.

**Recomendación:** no escribir desde cero un emulador PPU/SPU. Primero evaluar un launcher específico de Windows que use RPCS3 para la ejecución CPU/sistema/RSX, como dependencia externa o integrado solo después de revisar GPL-2.0-only. Es la ruta más rápida a un experimento ejecutable, pero puede conservar el consumo de RPCS3 y no se puede presentar como una solución de FPS. El probe Vulkan fuera de pantalla sigue siendo una prueba alternativa de gráficos, no el motor de ejecución PS3 propuesto. Medir rendimiento repetible en el PC objetivo antes de decidir si un subsistema propio podría corregir un cuello de botella demostrado. Consulta el [enfoque del runtime](approach.md). La decisión de construir un runtime jugable debe esperar mediciones representativas del juego.

## Evidence gates / Puertas de evidencia

| Gate | Result / Resultado | What it establishes / Qué demuestra |
| --- | --- | --- |
| Vulkan device API | PASS: Vega 8 reports Vulkan 1.2.170 | Adapter exposes the stated API floor. |
| Logical graphics device/queue | PASS | Driver accepts a Vulkan 1.2 graphics device and queue. |
| Win32 surface/present support | PASS | Adapter can present to a Windows surface. |
| One-frame swapchain and shader pipeline | PASS at 960×540 and 1280×720 | Basic graphics pipeline and configurable output size work; no sustained-performance result. |
| Vulkan Win32 swapchain | BLOCKED | FIFO presents three images, then acquisition times out; immediate mode crashes. Cause is not yet independently assigned to the driver or implementation. |
| Offscreen Vulkan + Windows output | PASS (synthetic only) | Clear/readback completes at 640×360 to 1280×720 output at roughly 49–75 frames/s across runs; no game workload or stable display pacing is established. |
| Windows launcher UI | PASS | Self-contained x64 app builds and starts; title-ID parser passes synthetic valid/malformed SFO fixtures; game launch through RPCS3 is not yet exercised. |
| Internal render resolution scaling | PARTIAL (synthetic only) | 640×360 internal image is scaled to a 1280×720 window; no game upscaler or image-quality evaluation yet. |
| Sustained game frame pacing / memory | NOT TESTED | CPU/SPU, RSX workloads, memory behavior, and representative gameplay remain unmeasured. |
| PPU/SPU translation | NOT TESTED | Main unknown for this four-core mobile CPU. |
| RSX compatibility with the game | NOT TESTED | Requires command/shader analysis and a title workload. |
| 50–60 FPS in representative gameplay | NOT TESTED | No performance guarantee can be made. |

## Performance bar / Umbral de rendimiento

50 FPS allows 20.0 ms per frame; 60 FPS allows 16.67 ms. A valid pass needs representative gameplay at the selected resolution, stable frame pacing, shader warm-up excluded or separately reported, and no recurring long stalls. A single clear or triangle is intentionally not treated as a game benchmark.

50 FPS permite 20,0 ms por frame; 60 FPS permite 16,67 ms. Para aprobar hacen falta escenas representativas del juego, frame pacing estable, calentamiento de shaders separado y ausencia de pausas largas recurrentes. Un clear o un triángulo aislado no cuenta como benchmark del juego.

## Sources / Fuentes

- [RPCS3 compatibility entry for God of War III](https://rpcs3.net/compatibility?g=BCAS25003&o=4d&type=1): the public snapshot lists `BCES00510` as **Ingame**, updated 2025-03-14. RPCS3 defines Ingame as titles with serious glitches, cannot-finish behavior, or insufficient performance. This is a compatibility warning, not a measurement on this PC or a claim about every current build.
- [RPCS3 issue #16909](https://github.com/RPCS3/rpcs3/issues/16909): a report on this title discusses SPURS settings and a CPU-related stutter on an i5-9400F. It is anecdotal, but consistent with CPU scheduling being a risk.
- [`gow3_pc` current README](https://github.com/FelipePrado0/gow3_pc): PS4 remaster target, x86-64 host execution path and Vulkan 1.3 requirement.
- [Khronos Vulkan Win32 surface extension](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_win32_surface.html)
- [Khronos Vulkan shader modules tutorial](https://github.khronos.org/Vulkan-Site/tutorial/latest/03_Drawing_a_triangle/02_Graphics_pipeline_basics/01_Shader_modules.html)
