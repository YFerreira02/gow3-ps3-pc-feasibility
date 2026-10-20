# Standalone runtime architecture / Arquitectura independiente

## Meaning of “native” / Qué significa “nativo”

The app will be a Windows-native executable and own its UI, input, audio, memory manager, game-specific compatibility services, and Vulkan renderer. To use a PS3 binary it must still translate/emulate the Cell CPU and PS3 platform. It is standalone; it is not direct x86 execution of the PS3 build. RPCS3 is explicitly not a component or subprocess.

La app será un ejecutable propio de Windows y controlará interfaz, entrada, audio, memoria, servicios de compatibilidad del juego y renderer Vulkan. Para usar un binario PS3 aún debe traducir/emular la CPU Cell y la plataforma PS3. Es independiente, pero no ejecuta el binario de PS3 directamente como x86. RPCS3 no será un componente ni subproceso.

## Components / Componentes

| Component | Responsibility | State / Estado |
|---|---|---|
| Windows host | Window, input, audio output, logging, settings, crash handling | Experimental Win32 GUI and console PPU host; file picker, bounded ELF execution, and status output only; no game audio/input/settings yet |
| Container/ELF readers | Locate EBOOT in an ISO9660 view; parse fixed SELF and bounded ELF/segment metadata; validate raw PPC64 big-endian ELF headers and PT_LOAD mapping; plain-only SELF PT_LOAD loader | ISO locator, SELF parser and plain-segment loader have synthetic checks. The user's ISO reports ELF entry `0x50ddc0`, 8 program headers, and encrypted nonempty program segments, which the loader refuses |
| PPU core | PowerPC 64-bit register/state model, instruction decoding, exceptions, translation blocks | C reference core executes a limited integer/FPU/VMX subset (six vector operations) with bounded run loop, CR/LR/CTR access, branches, byte/halfword/word/doubleword loads/stores, initial atomic `lwarx`/`stwcx.`, 128-byte `dcbz`, and memory barriers; atomic reservation is a same-word compare-and-swap approximation, not full Cell/multi-thread semantics |
| PPU dynarec | Compile verified PPU blocks to x86-64, cache/invalidate, map exceptions | Not started |
| PS3 services | Threads, virtual memory, filesystem, timers, synchronization, modules used by this title | Not implemented; a generic host callback can now receive `sc` and resume the PPU, but it does not implement any PS3 syscall |
| SPU runtime | SPU ISA, local store, DMA, mailboxes, events and scheduling | Not started; likely major CPU/performance risk |
| RSX frontend | Command processor, GPU state, resources, synchronization, shader semantics | Not started |
| Vulkan backend | Vulkan 1.2 device, pipeline, render-target and SPIR-V path, resolution scaling | Synthetic host probes exist; no RSX commands or game shaders |
| Title profile | Region/version boot setup and evidence-backed compatibility fixes | Not started; no guessed offsets/patches |

## Data flow / Flujo de datos

```text
user's ISO (read-only map) -> EBOOT locator -> SELF/ELF metadata inspection (implemented)
  -> plain-only SELF segment loader (synthetic tests only; retail segments encrypted)
  -> ELF validation and memory map (raw ELF path only)
  -> PPU translated blocks + PS3 services
  -> SPU translated workloads + scheduling
  -> RSX command/state translation
  -> Vulkan 1.2 resources, shaders, render targets
  -> Windows presentation
```

Only ISO lookup and SELF/ELF metadata inspection are implemented for the user's ISO. The plain-only loader is validated on synthetic fixtures and explicitly refuses the encrypted retail segments. Game PPU execution, services, SPU, RSX and Vulkan integration remain unfinished. The ISO is accessed by read-only memory mapping; no extracted or duplicate game files are created. No game content is tracked.

## Important limits / Límites

- Lower render resolution helps only a GPU-bound workload; it cannot fix CPU-bound PPU/SPU translation.
- The Vega 8 shares system memory. The supplied Task Manager snapshot showed about 92% RAM use, so memory budget and page pressure are concerns.
- A low-power Ryzen 5 3500U and this title's unknown SPU workload make 50–60 FPS a high-risk, unverified goal.
- Vulkan 1.2 is a required host API floor. Check the negotiated version and capabilities at runtime; do not silently require Vulkan 1.3.
- A synthetic clear or triangle is only a renderer smoke test.

## Inspiration / Inspiración

[`gow3_pc`](https://github.com/FelipePrado0/gow3_pc) targets the PS4 remaster and cannot execute this PS3 build. Its launcher/configuration/patch organization is useful as product inspiration. No source code has been reused. See [`approach.md`](approach.md) for license and implementation gates.
