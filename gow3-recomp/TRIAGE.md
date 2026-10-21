# Triaje del EBOOT — God of War III BCES00510 v1.00

Fecha: 2026-10-21. Herramientas: ps3recomp v0.17 (`triage_elf.py`, `gen_imports.py`,
`extract_spu_images.py`) más el registro de RPCS3 de la mejor partida medida
(v5-rsx-1hilo, 2026-10-21 00:30).

## Descifrado
- El EBOOT es un SELF de disco APP, revisión de clave 0x0001, firmware 1.00.
- Se descifró con `decrypt_self.py` (copiado de Ascension), usando las claves APP
  públicas del código fuente de RPCS3.
- El ELF resultante mide 5,46 MB. El punto de entrada resuelve a OPD func=0x10230,
  toc=0x52d6c8, así que el descifrado es correcto.
- SHA-1 del ELF: `0c5eff537abc7bbf0ec71f4c6c688e7023e0b615`. Sirve para comprobar
  que se trabaja siempre sobre el mismo ejecutable.

## Tamaño
| | God of War III |
|---|---|
| Código ejecutable | 5,02 MB (segmento de texto en 0x10000) |
| Datos | 0,27 MB en el archivo, 1,6 MB en memoria |
| Funciones estimadas | ~7.400 marcos de pila |
| Llamadas `bl` | 32.365 |

Para comparar: Ascension generó 1,8 millones de líneas de C++. Con un ejecutable de
este tamaño, lo esperable para God of War III es una cifra del mismo orden.

## Importaciones de firmware: 184 funciones en 13 librerías, 90% ya presentes en ps3recomp
| Librería | Presentes | Faltan |
|---|---|---|
| cellSpurs | 34/44 | `CreateTask2`, `JoinTask2`, `TryJoinTask2`, `DestroyTaskset2`, `_TaskAttribute2Initialize`, `GetWorkloadFlag`, `_WorkloadFlagReceiver`, `LFQueueDetachLv2EventQueue`, `_LFQueuePushBody`, NID 0x58D58FCF |
| sysPrxForUser | 24/30 | la familia `sys_prx_load_module*`, `sys_prx_register_library`, `sys_game_process_exitspawn2` |
| cellGcmSys | 27/28 | `_cellGcmFunc15` |
| sceNpTrophy | 10/11 | `sceNpTrophyAbortHandle` |
| cellSysutil, sys_fs, libvdec, sys_io, cellAudio, cellGame, sceNp, cellSysmodule, AvconfExt | todas | — |

La comprobación es aproximada: busca el nombre de cada función en el código de
ps3recomp. Lo que falta está casi todo en SPURS, en la API de tareas de segunda
generación. Es justo lo que Ascension ya amplió: sus parches 0001 (ya aplicado) y 0003
(`_LFQueuePushBody`) cubren parte. La familia `sys_prx_*` la usa liblv2 de forma
interna. El juego no carga PRX propias del disco: en el registro no aparece ninguna
`.sprx` de `/dev_bdvd`.

## SPU
- El EBOOT trae **8 programas SPU incrustados**, 485 KB en total. Van de 12 a 143 KB.
- En RPCS3, el juego compila **3.974 funciones SPU distintas**. La mayoría viene de
  código que se carga sobre la marcha desde los archivos del juego (tasksets y job
  chains), igual que los módulos de trabajo de Ascension.
- Usa SPURS completo: Initialize/InitializeWithAttribute/ForSpuSharing, workloads con
  atributos, tasksets (incluido Taskset2), Task2 y job chains (`RunJobChain`,
  `KickJobChain`, `JoinJobChain`).
- RPCS3 avisa de recíprocos y raíces aproximadas (`spu_re`, `spu_rsqrte`, `spu_fi`)
  dentro de operaciones FMA. Ascension resolvió esto con su modo de floats SPU
  aproximado (parches 0032 y 0024).

## Conclusión
- **Viable con el método de Ascension.** El PPU es manejable: 5 MB y 90% de
  cobertura. Las piezas que faltan son las mismas que Ascension ya tuvo que resolver.
- **El trabajo grande está en las SPU.** Las job chains no aparecen en Ascension. Habrá
  que extraer y traducir el código SPU que se carga desde los archivos del juego, y
  para eso hacen falta los archivos de `USRDIR` dentro de la ISO.
- **Estimación:** varias semanas para llegar al menú y meses para algo jugable, en
  línea con la guía de ps3recomp para un título AAA.

## Siguiente paso (fase 3)
1. Traducir el PPU con `ppu_loader.py` y `ppu_lifter.py`, aquí en la nube.
2. Traducir los 8 programas SPU incrustados.
3. Añadir las funciones SPURS que faltan, empezando por los parches de Ascension
   0003, 0008, 0011 y 0019 adaptados a v0.17.
4. Primer arranque: compilar aquí y llevar el `.exe` al PC.
