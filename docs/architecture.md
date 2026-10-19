# Standalone runtime architecture / Arquitectura independiente

## Meaning of “native” / Qué significa “nativo”

The app will be a Windows-native executable and own its UI, input, audio, memory manager, game-specific compatibility services, and Vulkan renderer. To use a PS3 binary it must still translate/emulate the Cell CPU and PS3 platform. It is standalone; it is not direct x86 execution of the PS3 build. RPCS3 is explicitly not a component or subprocess.

La app será un ejecutable propio de Windows y controlará interfaz, entrada, audio, memoria, servicios de compatibilidad del juego y renderer Vulkan. Para usar un binario PS3 aún debe traducir/emular la CPU Cell y la plataforma PS3. Es independiente, pero no ejecuta el binario de PS3 directamente como x86. RPCS3 no será un componente ni subproceso.

## Components / Componentes

| Component | Responsibility | State / Estado |
|---|---|---|
| Windows host | Window, input, audio output, logging, settings, crash handling | Existing synthetic Vulkan probes; no game UI/runtime yet |
| ELF parser/segment mapper | Validate PPC64 big-endian ELF header, segment bounds and executable entry; copy PT_LOAD bytes into supplied guest memory and zero BSS | Synthetic ELF program executes compare/branch flow; no path-based file loading, SELF, or decryption support |
| PPU core | PowerPC 64-bit register/state model, instruction decoding, exceptions, translation blocks | C reference core executes D-form and selected X-form integer operations, direct/conditional/indirect branches, and byte/word loads/stores; synthetic checks pass |
| PPU dynarec | Compile verified PPU blocks to x86-64, cache/invalidate, map exceptions | Not started |
| PS3 services | Threads, virtual memory, filesystem, timers, synchronization, modules used by this title | Not started; requires lawful decrypted executable evidence |
| SPU runtime | SPU ISA, local store, DMA, mailboxes, events and scheduling | Not started; likely major CPU/performance risk |
| RSX frontend | Command processor, GPU state, resources, synchronization, shader semantics | Not started |
| Vulkan backend | Vulkan 1.2 device, pipeline, render-target and SPIR-V path, resolution scaling | Synthetic host probes exist; no RSX commands or game shaders |
| Title profile | Region/version boot setup and evidence-backed compatibility fixes | Not started; no guessed offsets/patches |

## Data flow / Flujo de datos

```text
user-provided decrypted PS3 executable (future, not current ISO)
  -> ELF/container validation and memory map
  -> PPU translated blocks + PS3 services
  -> SPU translated workloads + scheduling
  -> RSX command/state translation
  -> Vulkan 1.2 resources, shaders, render targets
  -> Windows presentation
```

Every arrow is unfinished except isolated synthetic Vulkan host probes and the tiny PPU interpreter subset. The previously identified third-party ISO is not an input to development: do not move, copy, inspect, extract, decrypt, or execute it. Leave the ZIP where it is. No game content is tracked.

## Important limits / Límites

- Lower render resolution helps only a GPU-bound workload; it cannot fix CPU-bound PPU/SPU translation.
- The Vega 8 shares system memory. The supplied Task Manager snapshot showed about 92% RAM use, so memory budget and page pressure are concerns.
- A low-power Ryzen 5 3500U and this title's unknown SPU workload make 50–60 FPS a high-risk, unverified goal.
- Vulkan 1.2 is a required host API floor. Check the negotiated version and capabilities at runtime; do not silently require Vulkan 1.3.
- A synthetic clear or triangle is only a renderer smoke test.

## Inspiration / Inspiración

[`gow3_pc`](https://github.com/FelipePrado0/gow3_pc) targets the PS4 remaster and cannot execute this PS3 build. Its launcher/configuration/patch organization is useful as product inspiration. No source code has been reused. See [`approach.md`](approach.md) for license and implementation gates.
