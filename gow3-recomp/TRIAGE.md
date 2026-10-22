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

## Cuelgue intermitente (2026-10-21, noche)
- Foto del vigilante a los 120 s en dos cuelgues seguidos (20:11 y 20:18): idéntica.
  El hilo principal (guest-tid 1) está en `func_0028DA14`, en un bucle
  `usleep(30)` esperando que la media palabra de 16 bits en `0x649EC0 + 4*i`
  (i = cuadro % 3; base = `*(toc-0x4C64)` = `0x649E80`) vuelva a 0. Es el
  contador de sincronización de cada cuadro (`{u16 cuenta; u8 id; u8 estado}`,
  lo arman `func_0028D534`/`func_0028DB7C`). Lo esperable es que lo bajen los
  trabajos SPU al terminar; si una bajada se pierde, el juego se queda esperando.
- Siguiente medida: `SPU_ATOM_EA=649E80` (atómicos SPU sobre esa línea) y
  `PPU_WWATCH=0x649EC0` (escrituras PPU), vía `entorno.txt`.
- Resultado (20:24 y 20:41, ambas colgadas): cada cuadro el PPU pone a 1 la
  cuenta de su turno y la baja un único trabajo SPU por GETLLAR/PUTLLC, de una
  lista EDGE distinta por turno (turno 0 = wid 8, 1 = wid 9, 2 = wid 10). Los
  atómicos funcionan (los PUTLLC fallidos son de wid 7 y se reintentan bien).
  En el cuelgue el turno 0 queda en 1: la lista EDGE0 (wid 8) nunca ejecuta su
  trabajo de ese cuadro. No es una bajada perdida, es un trabajo que no corre.
- Siguiente medida: SPURS_MAXCONT vacío (1 SPU por lista) en vez de
  7=3,8=3,9=3,10=3, que en Ascension ya colgaba si se aplicaba a todas.
- Prueba 21:13 con SPURS_MAXCONT vacío (confirmado en arranque_*.txt): se cuelga
  igual (esta vez el turno 1, wid 9). MAXCONT queda descartado; el lanzador ya
  no lo pone por defecto.
- Siguiente medida: SPURS_KICKLOG (cada admisión con trabajo pendiente) y
  SPURS_PM_RUNLOG=8, para ver si la lista atascada tiene readyCount y no se
  admite, o no recibe aviso.
- Nota del puente de archivos: al reescribir un archivo con el mismo stagedPath
  a veces llega la versión anterior al PC. Usar una carpeta nueva por envío.
- Prueba 21:23 (KICKLOG + PM_RUNLOG=8): esta vez los tres contadores quedaron
  en 0 y el hilo principal estaba más adelante, en func_002EAFB0, que repite
  func_002E7B18(lista, 3, ...) hasta que la lista de trabajos llega al estado
  3. Hacia t=344928 ms el núcleo SPURS deja de admitir cualquier workload con
  trabajo pendiente (no hay más líneas [spurs-kick], aunque wid 6 tenía
  ready=1), mientras wid 7 sigue haciendo atómicos en 0x649E80. Hipótesis: las
  SPU quedan ocupadas por trabajos de RenderJobs (wid 7) que esperan girando, y
  las listas EDGE ya no consiguen SPU. Siguiente: SPURS_WLDUMP=15 (ocupación
  de cada SPU) y SPURS_PM_RUNLOG=7.
