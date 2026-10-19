# Feasibility record / Registro de viabilidad

## 1. Mission / Misión

### English

Determine whether a game-specific PC runtime for the PS3 release of *God of War III* can plausibly reach 50–60 FPS on the owner's Windows 11 PC, using Vulkan 1.2 or later and adjustable render resolution. The present project is a feasibility study, not a commitment to deliver a complete port.

### Español

Determinar si un runtime para PC específico para la versión de PS3 de *God of War III* podría alcanzar 50–60 FPS en el PC Windows 11 del usuario, usando Vulkan 1.2 o posterior y resolución de renderizado ajustable. El proyecto actual es un estudio de viabilidad, no un compromiso de entregar un port completo.

## 2. Agreed constraints / Condiciones acordadas

| Area | Current decision |
| --- | --- |
| Game edition | PlayStation 3 release |
| OS | Windows 11 only |
| Graphics API floor | Vulkan 1.2 |
| First hardware target | AMD Ryzen 5 3500U, Radeon Vega 8 iGPU, 10 GB RAM |
| Performance goal | 50–60 FPS; render resolution adjustable |
| Phase outcome | Viability research plus a small technical probe using the user's ISO |
| Repository | New private GitHub repository named `gow3-ps3-pc-feasibility` |
| Documentation | Spanish and English |
| Game files | Remain local; never add ISO, extracted game files, firmware, or copyrighted assets to Git |

## 3. Evidence collected / Evidencia reunida

- User reports an ISO at `C:\Games\God_Of_War_3` and has not extracted it.
- The directory also contains a similarly sized ZIP archive. Its relationship to the ISO has not been investigated.
- A temporary read-only Windows mount exposed a UDF volume named `PS3VOLUME`.
- Disc root entries observed: `PS3_GAME`, `PS3_UPDATE`, `PS3_DISC.SFB`.
- `PS3_GAME\USRDIR\EBOOT.BIN` exists and is approximately 5,465,224 bytes.
- The first four executable bytes were `53-43-45-00` (`SCE\0`), consistent with a SELF/container header. This alone does not identify the executable's decrypted architecture or code sections.
- The image was dismounted after inspection. No content was extracted or changed.
- `gow3_pc` describes itself as a PS4 Remastered port. Its README says the original x86-64 game code runs on the PC CPU and its graphics use shadPS4's Vulkan core. The PS3 edition is a different target and cannot reuse that executable/runtime directly.
- The reference PC's Task Manager screenshot reports Ryzen 5 3500U with Radeon Vega Mobile Graphics, Radeon Vega 8, 2 GB dedicated GPU memory and 5 GB shared GPU memory; it also reports 10 GB system memory. Another Radeon 530 Series device is shown as unavailable.
- The same screenshot shows 9.1/9.9 GB system memory in use (92%). That left roughly 0.8 GB available in that snapshot, a major practical constraint for a PS3 runtime, shader caches, and the game's working set. It is a snapshot, not a clean-boot baseline, so remeasure memory before making a final hardware verdict.
- Read-only local Windows inventory: build `26300.9711`, corresponding to Windows 11 26H2; Ryzen 5 3500U, 4 cores / 8 logical processors; approximately 9.90 GB RAM. Although the registry's `ProductName` said “Windows 10 Pro”, the build number identifies the Windows 11 26H2 family; use the build as the stronger signal and re-check via Settings before publishing system requirements.
- Read-only `vulkaninfo --summary`: the Vulkan loader is 1.3.301, while **both devices report device API 1.2.170**. Do not confuse the loader version with the GPU's supported API version. The Vega 8's AMD driver reports 26.5.2; the Radeon 530 reports 22.6.1.
- Feature spot-check on Vega 8: `bufferDeviceAddress`, `scalarBlockLayout`, `separateDepthStencilLayouts`, `shaderFloat16`, `shaderInt8`, `timelineSemaphore`, `descriptorIndexing`, `samplerFilterMinmax`, and `vulkanMemoryModel` report true. Radeon 530 reports false for `shaderFloat16`; Vega 8 is the intended initial adapter.
- `vulkaninfo` also printed an AMD `amdvlk64.dll` ICD `vkCreateInstance` warning (-13) and surface capability warnings while enumerating the devices. Device API support was reported, but the warning should be diagnosed in the prototype and must not be hidden from the final compatibility report.
- A project-owned PowerShell/C# interop probe now creates a Vulkan 1.2 instance through `vulkan-1.dll`, enumerates physical devices, reads their device API versions, and creates/destroys a logical device plus graphics queue on the requested adapter. It ran successfully on this PC: Vega 8 and Radeon 530 both returned `1.2.170`, and the Vega 8 graphics queue passed. This independent instance-creation path did not prevent usable adapter enumeration. The probe intentionally does not create a window/swapchain, render shaders, measure FPS, or validate any game behavior.
- A second probe created a hidden Win32 window handle, enabled `VK_KHR_surface` and `VK_KHR_win32_surface`, created a Vulkan surface, and checked presentation support. On this PC, Vega 8 surface creation passed and graphics queue family 0 can present to that surface.
- A third probe created a logical device with `VK_KHR_swapchain`, selected FIFO presentation, created a swapchain at 304x141 for the temporary hidden window, acquired one image, transitioned it to transfer-destination layout, cleared it, transitioned it to present layout, submitted the command buffer, and presented it. The full path returned success on Vega 8. This validates basic Vulkan image presentation, not shaders, resizing, sustained frame pacing, the game renderer, or performance.
- The shader-backed follow-on compiled a small vertex/fragment pair to SPIR-V targeting Vulkan 1.2, created shader modules and a graphics pipeline, rendered a triangle to a color-attachment swapchain, and presented it on Vega 8. It returned exit code 0 at both 960x540 and 1280x720. This is a one-frame API/pipeline smoke test only; its workload is not a performance proxy for the game.
- A later bounded multi-frame experiment acquired, submitted, and presented two images on both AMD adapters, then Windows recorded an `APPCRASH` in `amdvlk64.dll` (`0xc0000005`) while the third `vkAcquireNextImageKHR` was attempted. The same failure occurred on Vega 8 and Radeon 530. This is observed in the manual interop probe; without Vulkan validation layers and a separate native reproduction, we cannot attribute it conclusively to the driver rather than an interop defect.
- Khronos' swapchain guide recommends indexing presentation wait semaphores by the acquired swapchain image, because queue idle/fences do not prove that the presentation engine has finished consuming them. An experimental probe used this image-indexed design, but the third-acquire crash persisted. The local `vulkaninfo` output did not list `VK_KHR_swapchain_maintenance1` / `VK_EXT_swapchain_maintenance1` or `VK_LAYER_KHRONOS_validation`, so present fences and API validation were unavailable in the inspected environment.
- Cleanup limitation: on this AMD setup, explicitly destroying the just-presented swapchain caused the probe process to terminate abnormally. The short-lived probe therefore tears down its device/process without an explicit swapchain-destroy call. This is a prototype-only workaround, not an acceptable lifecycle for a production renderer; investigate WSI/presentation completion and validate with Vulkan validation layers before building a long-running application.
- The same shader pipeline also returned success with a 1280x720 swapchain. `-Width` and `-Height` therefore control output/swapchain resolution in the probe. An independent offscreen/internal render scale is not implemented yet.
- The custom Vulkan instance/surface/swapchain probes did not reproduce the `amdvlk64.dll` warning printed by `vulkaninfo`; they succeeded on the selected Vega 8 adapter. Keep the `vulkaninfo` warning in the record because it may correspond to another ICD path and has not been fully diagnosed.
- Read-only in-memory `PARAM.SFO` parse: title `God of War III`, title ID `BCES00510`, app version `01.00`, disc version `01.03`, required system version `03.1500`, category `DG`. The ISO was dismounted after the parse.
- Public RPCS3 compatibility data lists `BCES00510` as `Ingame`, updated 2025-03-14 in the observed snapshot. RPCS3 defines that status as serious glitches, inability to finish, or insufficient performance. Treat it as a compatibility/performance warning, not a measurement on this PC or a permanent verdict. See [RPCS3's compatibility page](https://rpcs3.net/compatibility?g=BCAS25003&o=4d&type=1) and its [status definitions](https://rpcs3.net/compatibility).
- An RPCS3 issue for this exact title documents stutter reports and CPU/SPURS scheduling observations on other hardware. This is anecdotal, but reinforces that a graphics-only Vulkan prototype cannot answer the main CPU risk. See [RPCS3 issue #16909](https://github.com/RPCS3/rpcs3/issues/16909).

## 4. Main feasibility questions / Preguntas principales

1. Can the PS3 executable be analyzed without distributing or relying on copyrighted game data?
2. Which PPU, SPU, RSX, audio, input, filesystem, and system-service features does the game use?
3. Can the CPU/SPU translation workload fit within the Ryzen 5 3500U's CPU budget at a 50–60 FPS target?
4. Does the Vega 8's installed driver expose the Vulkan 1.2 features and limits required by a candidate RSX renderer?
5. What renderer-specific workload and memory footprint are possible on an iGPU with shared system memory?
6. Are the game-specific patches known to exist and legally reusable, and what source/license obligations would follow?
7. Can the first technical prototype answer a high-value question without attempting to implement the whole game?

## 5. Preliminary risk register / Riesgos preliminares

| Risk | Why it matters | Current assessment |
| --- | --- | --- |
| PPU/SPU translation cost | PS3 code does not share the PC's x86-64 instruction set; SPU-heavy workloads can be CPU-bound. | High uncertainty; must be investigated before committing to a port. |
| RSX translation | PS3 graphics commands and shaders need their own renderer path; Vulkan does not translate RSX automatically. | Major engineering effort. |
| Reference PC performance | Ryzen 5 3500U and Vega 8 are low-power integrated components with shared memory. | 50–60 FPS is an ambitious target; not yet demonstrated. |
| Vulkan 1.2 capabilities | API version alone does not guarantee all needed limits/extensions or good driver performance. | Device API 1.2.170 and selected feature bits confirmed on Vega 8; full renderer requirements and driver warning still need investigation. |
| 50–60 FPS target | The title may be CPU/SPU-bound and its game logic may need a frame-rate patch. | High risk and wholly unproven on this PC; no FPS claim is justified yet. |
| Decryption and executable analysis | The observed SCE signature indicates a container, not that code can immediately be disassembled or run. | Need lawful, non-distributive analysis plan and metadata. |
| Scope creep | A complete game-specific runtime needs CPU, GPU, audio, input, OS services, save data, and compatibility fixes. | Keep phase 1 focused on evidence and a small probe. |
| GPL-2.0 reuse | `gow3_pc` declares GPL-2.0; adapting its code may impose source/license obligations on distribution. | Use as design reference until license decision/review. |

## 6. Proposed investigation sequence / Secuencia propuesta

1. **Complete:** record hardware, Windows build, drivers, Vulkan device API/feature spot-check, and disc identifiers without extracting files.
2. **Complete:** create and present a synthetic clear-color swapchain frame on Vega 8.
3. **Complete (single-frame only):** compile SPIR-V for Vulkan 1.2, create a graphics pipeline, draw a shader-backed triangle, and present it on Vega 8 at 960x540 and 1280x720.
4. **Blocked pending diagnostic toolchain:** reproduce the third-acquire crash with Khronos validation layers or a known-good native Vulkan sample. Keep the checked-in C# interop probe at one frame; do not use it for multi-frame timing. Determine whether the cause is interop/ABI handling, Windows WSI, or the installed AMD driver before changing teardown behavior.
5. After multi-frame presentation is stable, build a separate offscreen render target so internal resolution can vary independently of output resolution. Record frame-time distribution and memory. Remeasure available system memory from a clean boot. Investigate the `vulkaninfo` ICD warning if it affects the selected path. Do not run the game yet.
6. Map documented PPU/SPU/RSX requirements and the RPCS3 architecture to a game-specific runtime boundary; identify what can be reused under its license and what is proprietary game behavior.
7. Determine a lawful analysis path for the user's own dump and define a metadata-only report. Do not add executable contents, keys, firmware, or assets to Git.
8. Only after the renderer and toolchain are stable, define a small CPU/SPU translation experiment and a separate RSX workload estimate.
9. Reassess CPU, GPU, memory, performance goal, and engineering scope. State what evidence would change the estimate.
10. Make a go/no-go decision for a playable runtime. Require measured representative gameplay performance before claiming the 50–60 FPS target.

The current evidence-based recommendation is in the [bilingual decision memo](decision.md): proceed with narrow prototypes, but defer a full-port commitment and do not promise 50–60 FPS.

## 7. Out of scope for phase 1 / Fuera del alcance de la fase 1

- A playable or complete port.
- Promising 50–60 FPS before measurements.
- Including the game ISO, extracted assets, game executable, PS3 firmware, or proprietary symbols in GitHub.
- Reusing code from `gow3_pc` before documenting the license decision.
- Supporting operating systems other than Windows 11.
- Supporting graphics APIs below Vulkan 1.2.

## 8. Sources / Fuentes

To rerun the local capability probe from the repository root in Windows PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-Vulkan12.ps1 -PreferDevice "Vega 8"
powershell -STA -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-VulkanWin32Surface.ps1 -PreferDevice "Vega 8"
powershell -STA -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-VulkanSwapchain.ps1 -PreferDevice "Vega 8"
powershell -STA -NoProfile -ExecutionPolicy Bypass -File .\tools\Test-VulkanTriangle.ps1 -PreferDevice "Vega 8" -Width 960 -Height 540
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Compile-Shaders.ps1
```

The first three probes do not need the Vulkan SDK or a compiler installation; Windows PowerShell compiles small interop declarations at runtime. The triangle probe uses the checked-in GLSL/SPIR-V files and validates shader modules, graphics pipeline, and one swapchain presentation. Change `-Width` and `-Height` to choose another output size. This does not change an offscreen/internal render target yet, and the triangle does not test game compatibility or sustained performance. The probe's swapchain teardown workaround and multi-frame crash are documented above; neither is acceptable for a long-running runtime.

To rebuild the shader binaries using glslang, pass its executable path, for example `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Compile-Shaders.ps1 -CompilerPath "C:\Tools\glslang\bin\glslang.exe"`. The binaries were generated with Khronos glslang 16.6.0 for target `vulkan1.2`; the official Windows x64 release archive SHA-256 was `82bf434e69b9bb4829de7e2b4bc2c5e7a7861e53d66cf75e5cc70f5f694a8d9b`. Temporary compiler files are ignored under `work/`.

- Reference project: [`FelipePrado0/gow3_pc`](https://github.com/FelipePrado0/gow3_pc)
- Reference README: [`gow3_pc` README](https://github.com/FelipePrado0/gow3_pc#readme)
- RPCS3 project: [`RPCS3/rpcs3`](https://github.com/RPCS3/rpcs3)
- RPCS3 SPU LLVM recompiler (evidence that SPU execution involves translation/JIT): [SPULLVMRecompiler.cpp](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/SPULLVMRecompiler.cpp)
- RPCS3 SELF container parser: [unself.h](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Crypto/unself.h)
- shadPS4 user requirements (the reference project's PS4 path has a newer Vulkan requirement): [Quick start](https://github.com/shadps4-emu/shadPS4/wiki/I.-Quick-start-%5BUsers%5D/8eb552946733b22cdd78fddd9fd6eb197072e088)
- Microsoft Windows build mapping: [Windows Insider release notes](https://learn.microsoft.com/en-us/windows-insider/release-notes/release-preview-24h2-25h2/build-26100-9539-26200-9539)
- Khronos Vulkan Win32 surface extension: [VK_KHR_win32_surface](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_win32_surface.html)
- Khronos Vulkan surface/swapchain specification: [Vulkan Specification](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html)
- Khronos GLSL-to-SPIR-V compiler used for the sample: [glslang releases](https://github.com/KhronosGroup/glslang/releases)
- Triangle shader pipeline reference: [Khronos Vulkan tutorial](https://github.khronos.org/Vulkan-Site/tutorial/latest/03_Drawing_a_triangle/02_Graphics_pipeline_basics/01_Shader_modules.html)
- Presentation semaphore reuse: [Khronos Vulkan Guide](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html)
- Present fences / swapchain maintenance: [Khronos Vulkan Guide](https://docs.vulkan.org/features/latest/features/proposals/VK_EXT_swapchain_maintenance1.html)
- Validation layers: [LunarG Windows Vulkan SDK documentation](https://vulkan.lunarg.com/doc/view/latest/windows/getting_started.html)
- RPCS3 title-specific stutter / CPU scheduling report: [issue #16909](https://github.com/RPCS3/rpcs3/issues/16909)
