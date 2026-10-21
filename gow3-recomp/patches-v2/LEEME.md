# Kit actual (desde el 2026-10-21, quinta prueba)

El runtime es el de Ascension: **ps3recomp `ff66a35`** (16 oct. 2026) con sus 44
parches, más los cambios propios de God of War III. Todo va en un único diff exacto:

```
git clone https://github.com/sp00nznet/ps3recomp ps3kit
cd ps3kit && git checkout ff66a35
git apply ../gow3-recomp/patches-v2/gow3-kit-sobre-ff66a35.patch
```

- `ascension-originales/`: los 44 parches de Ascension tal como los publica, como referencia.
  Los 21 primeros entran con `git am --3way` sobre `ff66a35`; del 22 al 44 entran con
  `patch -F3` sin rechazos; hay que corregir a mano un bloque de `cellGcmSys.c` que
  queda detrás de un `else` sin llaves (ya hecho en el diff).
- Cambios propios dentro del diff: `_cellSpursWorkloadFlagReceiver`,
  `cellSpursGetWorkloadFlag`, `cellSpursTryJoinTask2`, `cellSpursDestroyTaskset2`,
  `cellSpursLFQueueDetachLv2EventQueue`, compilación con MinGW (sin SEH, getenv del CRT)
  y la arena de memoria con mmap en Linux.

Por qué se cambió: sobre ps3recomp v0.17 (carpeta `patches/`, la ruta anterior) el juego
llegó a la pantalla de brillo y después la GPU quedó parada en un salto a sí misma que
el posprocesado SPU nunca liberaba. Ese es justo el tramo GPU/SPU que Ascension resolvió.
