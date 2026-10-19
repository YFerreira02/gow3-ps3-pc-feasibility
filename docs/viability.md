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
- Read-only Windows system inventory identifies the laptop as Acer Aspire A315-41G. Its Vega 8 driver is `31.0.21925.1001`, which AMD maps to Adrenalin 26.5.2 for Polaris/Vega. AMD's release notes identify that as a Windows 11 package, but recommend OEM-customized drivers for laptops when generic packages cause compatibility issues. Before changing drivers, compare the installed package with the Acer-specific support listing; no driver was changed during this work. See [AMD's release notes](https://www.amd.com/en/resources/support-articles/release-notes/RN-RAD-WIN-26-5-2-POLARIS-VEGA.html), [AMD's 3500U driver page](https://www.amd.com/en/support/downloads/drivers.html/processors/ryzen/ryzen-3000-series/amd-ryzen-5-3500u.html), and [Acer's A315-41G support page](https://www.acer.com/es-es/support/product-support/Aspire_A315-41G/downloads).
- The same screenshot shows 9.1/9.9 GB system memory in use (92%). That left roughly 0.8 GB available in that snapshot, a major practical constraint for a PS3 runtime, shader caches, and the game's working set. It is a snapshot, not a clean-boot baseline, so remeasure memory before making a final hardware verdict.
- Read-only local Windows inventory: build `26300.9711`, corresponding to Windows 11 26H2; Ryzen 5 3500U, 4 cores / 8 logical processors; approximately 9.90 GB RAM. Although the registry's `ProductName` said “Windows 10 Pro”, the build number identifies the Windows 11 26H2 family; use the build as the stronger signal and re-check via Settings before publishing system requirements.
- Read-only `vulkaninfo --summary`: the Vulkan loader is 1.3.301, while **both devices report device API 1.2.170**. Do not confuse the loader version with the GPU's supported API version. The Vega 8's AMD driver reports 26.5.2; the Radeon 530 reports 22.6.1.
- Feature spot-check on Vega 8: `bufferDeviceAddress`, `scalarBlockLayout`, `separateDepthStencilLayouts`, `shaderFloat16`, `shaderInt8`, `timelineSemaphore`, `descriptorIndexing`, `samplerFilterMinmax`, and `vulkanMemoryModel` report true. Radeon 530 reports false for `shaderFloat16`; Vega 8 is the intended initial adapter.
- `vulkaninfo` also printed an AMD `amdvlk64.dll` ICD `vkCreateInstance` warning (-13) and surface capability warnings while enumerating the devices. Device API support was reported, but the warning should be diagnosed in the prototype and must not be hidden from the final compatibility report.
- A project-owned PowerShell/C# interop probe now creates a Vulkan 1.2 instance through `vulkan-1.dll`, enumerates physical devices, reads their device API versions, and creates/destroys a logical device plus graphics queue on the requested adapter. It ran successfully on this PC: Vega 8 and Radeon 530 both returned `1.2.170`, and the Vega 8 graphics queue passed. This independent instance-creation path did not prevent usable adapter enumeration. The probe intentionally does not create a window/swapchain, render shaders, measure FPS, or validate any game behavior.
- A second probe created a hidden Win32 window handle, enabled `VK_KHR_surface` and `VK_KHR_win32_surface`, created a Vulkan surface, and checked presentation support. On this PC, Vega 8 surface creation passed and graphics queue family 0 can present to that surface.
- A third probe created a logical device with `VK_KHR_swapchain`, selected FIFO presentation, created a swapchain at 304x141 for the temporary hidden window, acquired one image, transitioned it to transfer-destination layout, cleared it, transitioned it to present layout, submitted the command buffer, and presented it. The full path returned success on Vega 8. This validates basic Vulkan image presentation, not shaders, resizing, sustained frame pacing, the game renderer, or performance.
- The shader-backed follow-on compiled a small vertex/fragment pair to SPIR-V targeting Vulkan 1.2, created shader modules and a graphics pipeline, rendered a triangle to a color-attachment swapchain, and presented it on Vega 8. It returned exit code 0 at both 960x540 and 1280x720. This is a one-frame API/pipeline smoke test only; its workload is not a performance proxy for the game.
- The official Vulkan SDK 1.4.363.0 and `VK_LAYER_KHRONOS_validation` were installed locally for diagnostics (not part of the repository). Validation found an invalid `stencilStoreOp` enum in the hand-written interop; this was corrected. On the one-frame 960x540 and 1280x720 runs, no enum, semaphore, or other validation errors remain, but device teardown reports one leaked `VkSwapchainKHR`.
- A follow-up probe uses per-acquired-image render semaphores, as recommended by the [Khronos swapchain semaphore guide](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html). With validation enabled, it acquires, submits, and presents image 0 and image 1, then the process terminates with Windows status `0xC0000005` while requesting the third image. That is consistent with the earlier failure, but the C# P/Invoke interop has not been independently cleared; this is not enough evidence to assign fault to AMD's driver.
- An independent native C reproduction was compiled against the official Vulkan SDK headers and calls Vulkan without the handwritten C# Vulkan interop. The probe now enables `VK_EXT_debug_utils` and forwards warning/error messages from `VK_LAYER_KHRONOS_validation` through a debug messenger. On the Vega 8, validation-enabled runs presented two images; the three-frame run then terminated during the third acquire without a validation warning/error callback first. Windows Error Reporting records `powershell.exe` faulting in `amdvlk64.dll` version `31.0.21925.1001`, exception `0xC0000005`. A two-frame run also completes both presents and faults at `vkDestroySwapchainKHR`; a 1.5-second delay before destruction does not prevent it. `vulkaninfoSDK` does not list `VK_EXT_swapchain_maintenance1` or `VK_KHR_present_wait` for the enumerated devices. This makes a C# ABI issue less likely, but does not prove the hand-written native command sequence is valid or establish a general AMD-driver defect. The source and build/runner scripts are in `tools/VulkanNativePresentProbe.c`, `tools/Build-VulkanNativePresentProbe.ps1`, and `tools/Run-VulkanNativePresentProbe.ps1`.
- The WSI probe was tightened to use three swapchain images, a finite two-second acquire timeout, per-image command buffers/semaphores, semaphore recycling after reacquiring the corresponding image, and a Windows message pump. A synchronous diagnostic mode also waits for rendering before present and omits the present wait semaphore. On the Vega 8, FIFO mode presents three frames and then returns `VK_TIMEOUT` at the next acquire; the same timeout occurs in synchronous mode. The immediate present mode is enumerated but crashes in `amdvlk64.dll` during the first present; mailbox is unavailable and falls back to FIFO. Validation reports the forward-progress rule on the deliberately acquire-only path. Explicit cleanup after the WSI stall also faults in the driver. These results narrow the problem to the current Win32 WSI/AMD presentation path on this machine, but still do not prove a general driver defect. Do not use this probe as a runtime or performance test.
- A separate offscreen Vulkan 1.2 probe avoids surfaces and swapchains: it clears an optimal-tiled RGBA8 image, copies it to a mapped host-visible buffer, waits on a fence, and sends the pixels to a WinForms picture box for Windows-side scaling. It has a validation debug messenger. On Vega 8 it completed 60 frames at 640×360 internal resolution shown in a 1280×720 window with validation enabled and no warning/error callback. Measured render/readback/managed-copy throughput varied by run: approximately 49–75 frames/s at 640×360 to 1280×720 output (validation enabled/disabled); approximately 33 frames/s at 960×540 and 20 frames/s at 1280×720 internal/output in separate non-validation runs. These are clear-color and copy tests, not actual display-present counts, game rendering, or gameplay FPS. The runner drops stale UI frames when it falls behind. This is a fallback worth testing with a more representative Vulkan workload; the cost grows rapidly with internal resolution. Sources: `tools/VulkanOffscreenPresentProbe.c`, `tools/Build-VulkanOffscreenProbe.ps1`, and `tools/Run-VulkanOffscreenPresentProbe.ps1`.
- Explicitly destroying the just-presented swapchain also causes abnormal process termination on this setup, even after `vkDeviceWaitIdle`. Omitting that destroy avoids the termination but validation reports the leaked swapchain. Thus the current probe is a diagnostic smoke test, not a lifecycle-safe renderer. A native Vulkan reproduction and presentation-completion handling are required before proceeding to a long-running renderer.
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
4. **Blocked on host WSI:** Khronos-style semaphore handling, three swapchain images, a finite acquire timeout, a Windows message pump, and synchronous rendering-before-present still fail to provide forward progress on the current AMD driver. FIFO reaches three presents then times out; immediate presentation crashes. Do not infer a universal AMD defect. Do not use this path for FPS claims.
5. **Prototype fallback passed (synthetic only):** offscreen Vulkan 1.2 rendering/readback plus Windows-side bitmap scaling is stable for 60 validation-enabled frames. At 640×360 internal / 1280×720 output, measured clear/copy throughput is roughly 49–75 frames/s across runs; it falls to about 33 at 960×540 and 20 at 1280×720. Build a representative graphics workload and measure the actual UI paint rate/frame times before deciding whether to keep this output path. Remeasure memory after a clean boot and investigate the `vulkaninfo` ICD warning if it affects the selected path. Do not run the game yet.
6. Map documented PPU/SPU/RSX requirements and the RPCS3 architecture to a game-specific runtime boundary; identify what can be reused under its license and what is proprietary game behavior.
7. Determine a lawful analysis path for the user's own dump and define a metadata-only report. Do not add executable contents, keys, firmware, or assets to Git.
8. Only after the renderer and toolchain are stable, define a small CPU/SPU translation experiment and a separate RSX workload estimate.
9. Reassess CPU, GPU, memory, performance goal, and engineering scope. State what evidence would change the estimate.
10. Make a go/no-go decision for a playable runtime. Require measured representative gameplay performance before claiming the 50–60 FPS target.

## 6.1 RPCS3 launcher smoke test / Prueba rápida del launcher RPCS3

On 2026-10-19, the Windows x64 launcher was rebuilt from source and opened successfully. The published local package is `work/Gow3Launcher-win-x64.zip`; it contains the launcher and Vulkan probe, not RPCS3, firmware, or game data. Its per-title profile enables the FPS/frame-time overlay and selects Vulkan plus the Vega 8 adapter. The profile writer was separately exercised for initial creation, scale updates, backup behavior, idempotency, and rejection of an out-of-range scale.

A synthetic, asset-free RPCS3 homebrew fixture was attempted to check the next integration step. RPCS3 stopped before boot because no PS3 firmware was installed. Its command line supports `--installfw <path>`, but the official Sony firmware endpoint returned a TLS certificate-name mismatch from this Windows environment, so no download or certificate bypass was used. A later memory snapshot showed about 2.0 GB available out of 9.9 GB. No commercial game executable was launched, and no GOW3 FPS or compatibility result was obtained. Next test: install firmware obtained directly from Sony, rerun the synthetic fixture, then request a lawfully extracted game folder from the user for any title-specific test. Keep firmware and all game data outside Git.

El 2026-10-19 se recompiló el launcher x64 de Windows y se confirmó que abre. El paquete local está en `work/Gow3Launcher-win-x64.zip`; incluye el launcher y el probe Vulkan, pero no RPCS3, firmware ni datos del juego. El perfil por título activa el overlay de FPS/frame time y selecciona Vulkan y la Vega 8. También se probó por separado el escritor del perfil: creación, cambio de escala, respaldo, idempotencia y rechazo de escalas fuera del rango.

Se intentó el siguiente paso de integración con una fixture homebrew sintética y sin recursos del juego. RPCS3 se detuvo antes de arrancar porque no había firmware PS3 instalado. La línea de comandos admite `--installfw <path>`, pero el endpoint oficial de firmware de Sony devolvió desde este Windows un error de nombre de certificado TLS; no se descargó el archivo ni se omitió la validación del certificado. Una medición posterior mostró unos 2,0 GB disponibles de 9,9 GB. No se arrancó ningún ejecutable del juego comercial y no se obtuvo ningún resultado de compatibilidad o FPS de GOW3. Siguiente prueba: instalar firmware obtenido directamente de Sony, repetir la fixture sintética y solicitar una carpeta del juego extraída legalmente para cualquier prueba específica. Firmware y datos del juego deben seguir fuera de Git.

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

The first three probes do not need the Vulkan SDK or a compiler installation; Windows PowerShell compiles small interop declarations at runtime. The triangle probe uses the checked-in GLSL/SPIR-V files and validates shader modules, graphics pipeline, and one-frame swapchain presentation. The native C WSI probe is diagnostic only: with the revised three-image setup it times out after three FIFO presents, and cleanup may crash the current driver. For a native C cross-check, install Zig 0.15.2 and the Vulkan SDK, then build and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Build-VulkanNativePresentProbe.ps1 -ZigExe "C:\Tools\zig\zig.exe"
powershell -STA -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-VulkanNativePresentProbe.ps1 -Frames 4 -PreferDevice "Vega 8" -PresentMode fifo -Validation
```

The native probe requests a visible 960×540 Win32 window and presents a synthetic clear for each requested frame. On this PC it reaches three FIFO presents, then the next acquire times out; teardown after this WSI stall may also crash in the AMD driver. It is a diagnostic only and not a game benchmark or production renderer.

To build the swapchain-free Vulkan/GDI fallback probe and show a 640×360 internal image in a 1280×720 window:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Build-VulkanOffscreenProbe.ps1 -ZigExe "C:\Tools\zig\zig.exe"
powershell -STA -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-VulkanOffscreenPresentProbe.ps1 -Frames 300 -Width 640 -Height 360 -OutputWidth 1280 -OutputHeight 720 -Validation
```

`-Width`/`-Height` set the Vulkan internal image dimensions; `-OutputWidth`/`-OutputHeight` set the Windows display window size. The displayed image is copied through host memory and scaled by WinForms; this is a workaround experiment, not a production compositor or a game benchmark. The runner reports synthetic render/readback throughput and explicitly labels it as not a game FPS result.

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
