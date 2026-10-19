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
- Read-only local Windows inventory: build `26300.9711`, corresponding to Windows 11 26H2; Ryzen 5 3500U, 4 cores / 8 logical processors; approximately 9.90 GB RAM. Although the registry's `ProductName` said “Windows 10 Pro”, the build number identifies the Windows 11 26H2 family; use the build as the stronger signal and re-check via Settings before publishing system requirements.
- Read-only `vulkaninfo --summary`: the Vulkan loader is 1.3.301, while **both devices report device API 1.2.170**. Do not confuse the loader version with the GPU's supported API version. The Vega 8's AMD driver reports 26.5.2; the Radeon 530 reports 22.6.1.
- Feature spot-check on Vega 8: `bufferDeviceAddress`, `scalarBlockLayout`, `separateDepthStencilLayouts`, `shaderFloat16`, `shaderInt8`, `timelineSemaphore`, `descriptorIndexing`, `samplerFilterMinmax`, and `vulkanMemoryModel` report true. Radeon 530 reports false for `shaderFloat16`; Vega 8 is the intended initial adapter.
- `vulkaninfo` also printed an AMD `amdvlk64.dll` ICD `vkCreateInstance` warning (-13) and surface capability warnings while enumerating the devices. Device API support was reported, but the warning should be diagnosed in the prototype and must not be hidden from the final compatibility report.
- Read-only in-memory `PARAM.SFO` parse: title `God of War III`, title ID `BCES00510`, app version `01.00`, disc version `01.03`, required system version `03.1500`, category `DG`. The ISO was dismounted after the parse.
- Public RPCS3 compatibility data has listed `BCES00510` as `Ingame` in a dated snapshot, which is a warning about known compatibility/performance gaps, not a current definitive verdict. Compatibility status can change with emulator revisions. See [RPCS3's compatibility page](https://rpcs3.net/compatibility?g=BCAS25003&o=4d&type=1) and its [status definitions](https://rpcs3.net/compatibility).

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
2. **Next:** design and build a Vulkan 1.2 capability/renderer micro-prototype on Vega 8. First resolve the AMD ICD warning, then test device selection, swapchain, shader compilation, memory budget, and adjustable internal resolution with a synthetic scene. Do not benchmark or run the game yet.
3. Map documented PPU/SPU/RSX requirements and the RPCS3 architecture to a game-specific runtime boundary; identify what can be reused under its license and what is proprietary game behavior.
4. Determine a lawful analysis path for the user's own dump and define a metadata-only report. Do not add executable contents, keys, firmware, or assets to Git.
5. Only after the renderer and toolchain are stable, define a small CPU/SPU translation experiment and a separate RSX workload estimate.
6. Reassess CPU, GPU, memory, performance goal, and engineering scope. State what evidence would change the estimate.
7. Make a go/no-go decision for a playable runtime. Require measured representative gameplay performance before claiming the 50–60 FPS target.

## 7. Out of scope for phase 1 / Fuera del alcance de la fase 1

- A playable or complete port.
- Promising 50–60 FPS before measurements.
- Including the game ISO, extracted assets, game executable, PS3 firmware, or proprietary symbols in GitHub.
- Reusing code from `gow3_pc` before documenting the license decision.
- Supporting operating systems other than Windows 11.
- Supporting graphics APIs below Vulkan 1.2.

## 8. Sources / Fuentes

- Reference project: [`FelipePrado0/gow3_pc`](https://github.com/FelipePrado0/gow3_pc)
- Reference README: [`gow3_pc` README](https://github.com/FelipePrado0/gow3_pc#readme)
- RPCS3 project: [`RPCS3/rpcs3`](https://github.com/RPCS3/rpcs3)
- RPCS3 SPU LLVM recompiler (evidence that SPU execution involves translation/JIT): [SPULLVMRecompiler.cpp](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/SPULLVMRecompiler.cpp)
- RPCS3 SELF container parser: [unself.h](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Crypto/unself.h)
- shadPS4 user requirements (the reference project's PS4 path has a newer Vulkan requirement): [Quick start](https://github.com/shadps4-emu/shadPS4/wiki/I.-Quick-start-%5BUsers%5D/8eb552946733b22cdd78fddd9fd6eb197072e088)
- Microsoft Windows build mapping: [Windows Insider release notes](https://learn.microsoft.com/en-us/windows-insider/release-notes/release-preview-24h2-25h2/build-26100-9539-26200-9539)
