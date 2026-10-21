# Plan: God of War III recompilado

Las fases siguen el camino que recorrió Ascension (ver su `docs/MILESTONES.md`). Cada
fase termina con una prueba concreta en el PC.

## Fase 0 — Preparación (hecha)
- ps3recomp v0.17 (commit `a6d76c8`) con los 9 parches de Ascension que aplican limpio.
- El esqueleto del proyecto y los scripts de preparación y triaje.

## Fase 1 — Descifrar el EBOOT
Necesitas tu copia BCES00510 instalada en RPCS3, que ya tienes en `C:\Games\Emulador`.

1. En RPCS3, abre el menú **Utilidades → Descifrar binarios de PS3** (*Utilities →
   Decrypt PS3 Binaries*).
2. Elige `PS3_GAME\USRDIR\EBOOT.BIN` del juego.
3. RPCS3 deja `EBOOT.elf` en la misma carpeta, usando el firmware que ya tiene
   instalado. No hacen falta claves externas.
4. Cópialo a `work\EBOOT.elf`.

Ten en cuenta:
- La versión importa. Ascension usa la 1.00 del disco, y esta es la que tienes tú
  (v01.00). Si actualizas a la v1.03, cambia el ejecutable y habría que traducir todo
  de nuevo. Por eso conviene quedarse en la 1.00 para este proyecto.
- Además del EBOOT, hacen falta los módulos SPU y las PRX que estén en `USRDIR`. El
  script de triaje las lista.

## Fase 2 — Triaje
`triage.ps1` usa las herramientas de ps3recomp para medir:
- El tamaño del código ejecutable.
- Las importaciones de librerías del sistema, y cuántas ya tiene ps3recomp.
- El uso de SPU, SPURS y red.

Se compara con Ascension: unos 1,8 millones de líneas de C++ y 33 módulos de trabajo
SPU. Este paso es el que decide si seguir adelante.

## Fase 3 — Traducir y compilar
- Traducir el PPU con `ppu_lifter.py` y el SPU con `spu_lifter.py` y `lift_jobmods.py`.
- Compilar con Visual Studio 2022 (clang-cl), CMake y Ninja.
- El build de Ascension pide **16 GB de RAM** y el portátil tiene 12. Hay dos opciones:
  - Compilar en la nube y llevar al PC solo el `.exe`.
  - Ampliar el archivo de paginación de Windows. Es más lento, pero funciona.
- Primera meta: que el ejecutable arranque y pase las comprobaciones del sistema.

## Fase 4 — Del arranque al menú
Estos son los bloqueos que Ascension resolvió en este tramo y que God of War III
probablemente comparte:
- La estructura de `sys_lwcond_t`, los archivos de más de 2 GB y Edge Zlib en el host.
- SPURS2: 32 workloads, `tempArea` y los tasksets (arranques duplicados, tamaño de
  Taskset2).
- La sincronización GPU/SPU para el postproceso.
- Los videos con `cellVdec` y FFmpeg.

Meta: las pantallas de inicio, la carga y el menú principal.

## Fase 5 — El primer nivel (el Monte Olimpo)
- Gráficos con el RSX de RPCS3 (Vulkan). Es la vía que mejor dibuja en Ascension, y la
  Vega 8 cumple con Vulkan 1.2.
- Meta: que se pueda jugar el primer nivel, aunque vaya lento.

## Fase 6 — Rendimiento
- Fijar cada SPU a un núcleo y el hilo PPU al núcleo 0. En Ascension esto subió de
  14,8 a 16,7 FPS.
- Floats SPU "aproximados" y sin diagnósticos en el código traducido.
- Reescribir a mano en código nativo los trabajos SPU más pesados. Primero se graban
  trabajos reales del juego y se comprueba que el resultado sea idéntico byte a byte.
  En God of War III el candidato obvio es el MLAA.

## Qué esperar
- **Tiempo:** el autor de Ascension, con experiencia, tardó una semana en llegar al
  primer nivel. Aquí lo realista son semanas o meses.
- **Rendimiento:** Ascension recompilado corre el primer nivel a ~16–17 FPS. El límite
  es el código SPU, no la GPU. En el Ryzen 5 3500U lo probable es que, al principio, vaya
  igual o peor que RPCS3 (17 FPS). Solo se gana de verdad en la fase 6.
- **Mientras tanto:** se sigue jugando con `rpcs3-tuning/`.
