# Optimización de God of War III en RPCS3 (BCES00510)

**Equipo:** Ryzen 5 3500U (4 núcleos / 8 hilos), Radeon Vega 8 integrada y Radeon 530 dedicada, ~10 GB RAM, Windows 11, Vulkan 1.2.
**Emulador:** RPCS3 0.0.43-20355, instalado en `C:\Games\Emulador`.

Esta carpeta contiene un lanzador que aplica perfiles de configuración versionados y mide los FPS. Cada versión cambia una sola idea respecto a la anterior, para saber qué ayuda y qué no. Los archivos del juego y del firmware no se tocan ni se suben a Git.

## Uso en el PC

Copia esta carpeta en `C:\Games\GoW3_Tuning` y cierra RPCS3 antes de usarla.

| Archivo | Para qué |
|---|---|
| `Medir perfil.bat` | Elige un perfil, abre el juego y mide. Al inicio indicas cuántos segundos esperar (Enter = 120) para cargar tu partida y llegar a la escena; la medición empieza sola, dura 120 s y al terminar el lanzador cierra el juego. F10 (Fn + F10) también sirve para empezar antes. |
| `Jugar GoW3.bat` | Abre el juego con el mejor perfil hasta ahora (`best.txt`), sin medir. |
| `Restaurar config original.bat` | Devuelve la configuración del juego a como estaba antes de la primera prueba. |

Los resultados quedan en el PC en `results\<perfil>_<fecha>\` (en GitHub se suben solo `summary.txt` y `samples.csv`): `summary.txt` (FPS promedio, 5 % más bajo, mínimo, CPU y RAM libre), `samples.csv` (una muestra por segundo) y una copia de `RPCS3.log`.

**Escena de prueba:** siempre la misma, por ejemplo cargar la misma partida guardada y jugar el mismo tramo durante los 120 s. Si cambia la escena, los números no son comparables.

## Perfiles

Se generan con `python3 make_profiles.py` a partir de `profiles/base_BCES00510.yml`, que es la configuración que tenía el juego el 2026-10-20.

| Versión | Cambio respecto a la anterior |
|---|---|
| v0-actual | Ninguno: es la línea base. |
| v1-spu-mega | `SPU Block Size`: Safe → Mega |
| v2-spu-loop | `SPU loop detection`: activado |
| v3-720p | Resolución 720x480 → 1280x720 (la original de PS3) |
| v4-sched-alt | Desde v3: planificador de hilos "RPCS3 Alternative Scheduler" |
| v5-rsx-1hilo | Desde v3: `Multithreaded RSX` apagado |
| v6-spurs4 | Desde v3: `Max SPURS Threads` 6 → 4 |
| v7-radeon530 | Desde v3: GPU dedicada Radeon 530 en vez de la Vega 8 |

Después, combinar lo que funcione y recuperar efectos visuales (bloom, niebla, corrección de color) manteniendo apagado el MLAA de las SPU, que es el que más CPU consume.

## Resultados

Las cifras usan solo los segundos de **juego real**: CPU al 85 % o más y menos de 29.5 FPS. Los menús, las cargas y los momentos a 60 FPS inflan el promedio bruto y no se cuentan. Desde 2026-10-20 el lanzador calcula esto solo (`gameplay_*` en `summary.txt`).

| Versión | FPS juego (mediana) | FPS juego (promedio) | 5 % más bajo | Segundos de juego | Nota |
|---|---|---|---|---|---|
| v0-actual | 11.7 | 12.3 | 8.9 | 78 de 78 | Línea base, 720x480. CPU al 96 %: el límite es la CPU. |
| v1-spu-mega | 6.8 | 7.0 | 1.7 | 36 de 40 | **No válida.** Cambiar el tamaño de bloque SPU obliga a recompilar la caché SPU, y esta sesión pasó compilando. v2 incluye el mismo cambio y no muestra el problema. |
| v2-spu-loop | 12.2 | 13.5 | 9.2 | 62 de 84 | Igual o apenas mejor que v0, dentro del margen de ruido. |
| v3-720p | 12.6 | 13.3 | 9.5 | 35 de 81 | **1280x720 sin costo medible** frente a 720x480. Gran parte de la sesión fue en menús. Pasa a ser el perfil para jugar (`best.txt`). |

**Conclusión hasta ahora:** el juego va a unos 12–13 FPS en juego real con cualquiera de estos perfiles. Como la GPU no es el límite, la resolución original no cuesta FPS. Las siguientes pruebas (v4–v7) parten de v3 y cambian una sola cosa cada una, para atacar la CPU.

**Consejo para medir:** tras cambiar ajustes de SPU, la primera sesión compila caché y sale más lenta. Si un perfil nuevo da cifras muy bajas, repítelo una vez.
