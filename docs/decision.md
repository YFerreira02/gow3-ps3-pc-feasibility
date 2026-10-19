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
- **The 50–60 FPS goal is unverified, not disproved.** The Ryzen 5 3500U/Vega 8 can present the test pipeline, but this says nothing quantitative about the game. Lowering output resolution helps only if GPU rendering is a bottleneck; it will not fix PPU/SPU CPU saturation.

**Recommendation:** keep investing only in evidence-producing prototypes. First fix and validate swapchain/presentation teardown; then build a stable multi-frame synthetic renderer with independent internal/output resolutions and timing, followed by a small lawful CPU/SPU translation benchmark. The go/no-go for a playable game-specific runtime must wait for representative game-workload evidence.

### Español

- **SEGUIR:** continuar un prototipo pequeño del runtime/render de PC. En el equipo de referencia, Vulkan 1.2 ya crea instancia, dispositivo y cola gráfica; crea una superficie Win32 y un swapchain FIFO; construye un pipeline básico de vertex/fragment shaders; dibuja un triángulo sintético y lo presenta en una sola imagen a 960×540 y 1280×720. Es una prueba de un frame.
- **NO comprometerse todavía con un port completo:** no se ha demostrado la meta de 50–60 FPS. No se ha ejecutado código PS3, medido traducción PPU/SPU ni traducido comandos/shaders RSX del juego. El triángulo solo comprueba que funciona un pipeline Vulkan sencillo en el PC.
- **Riesgo alto.** La versión de PS3 necesita traducción PPU y SPU específica o reutilizada, servicios de PS3 y un renderer RSX. Una aplicación independiente de Windows todavía tendría que traducir/emular esas interfaces de PS3; Vulkan 1.2 no elimina ese trabajo de CPU. `gow3_pc` sirve como referencia de estructura del producto, pero apunta a la remasterización de PS4 y sus requisitos actuales indican Vulkan 1.3.
- **La meta de 50–60 FPS no está verificada, pero tampoco refutada.** Ryzen 5 3500U/Vega 8 puede presentar el pipeline sintético, pero eso no cuantifica el rendimiento del juego. Reducir la resolución de salida solo ayuda si el cuello de botella es la GPU; no resuelve saturación de CPU por traducción PPU/SPU.

**Recomendación:** invertir por ahora solo en prototipos que produzcan evidencia. Primero corregir y validar el cierre del swapchain/presentación; después crear un renderer sintético estable de varios frames que separe resolución interna y de salida, con mediciones de tiempo. Luego medir un caso pequeño de traducción CPU/SPU por una vía legal. La decisión de construir un runtime jugable debe esperar mediciones representativas del juego.

## Evidence gates / Puertas de evidencia

| Gate | Result / Resultado | What it establishes / Qué demuestra |
| --- | --- | --- |
| Vulkan device API | PASS: Vega 8 reports Vulkan 1.2.170 | Adapter exposes the stated API floor. |
| Logical graphics device/queue | PASS | Driver accepts a Vulkan 1.2 graphics device and queue. |
| Win32 surface/present support | PASS | Adapter can present to a Windows surface. |
| One-frame swapchain and shader pipeline | PASS at 960×540 and 1280×720 | Basic graphics pipeline and configurable output size work; no sustained-performance result. |
| Internal render resolution scaling | NOT TESTED | Need an offscreen image and upscale/composite path. |
| Sustained frame pacing / memory | NOT TESTED | Need validated swapchain teardown, multi-frame instrumentation, and memory accounting. |
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
