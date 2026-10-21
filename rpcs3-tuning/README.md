# Optimización de God of War III en RPCS3 (BCES00510)

**Equipo:** Ryzen 5 3500U (4 núcleos / 8 hilos), Radeon Vega 8 integrada y Radeon 530 dedicada, ~10 GB RAM, Windows 11, Vulkan 1.2.
**Emulador:** RPCS3 0.0.43-20355, instalado en `C:\Games\Emulador`.

Esta carpeta contiene un lanzador que aplica perfiles de configuración versionados y mide los FPS. Cada versión cambia una sola idea respecto a la anterior, para saber qué ayuda y qué no. Los archivos del juego y del firmware no se tocan ni se suben a Git.

## Uso en el PC

Copia esta carpeta en `C:\Games\GoW3_Tuning` y cierra RPCS3 antes de usarla.

| Archivo | Para qué |
|---|---|
| `Medir perfil.bat` | Elige un perfil, abre el juego y mide. En la escena de prueba pulsa **F10** (en portátil, **Fn + F10**), o elige al inicio que la medición empiece sola tras N segundos: mide 120 s y suena un pitido al terminar. Luego cierra el juego. |
| `Jugar GoW3.bat` | Abre el juego con el mejor perfil hasta ahora (`best.txt`), sin medir. |
| `Restaurar config original.bat` | Devuelve la configuración del juego a como estaba antes de la primera prueba. |

Los resultados quedan en `results\<perfil>_<fecha>\`: `summary.txt` (FPS promedio, 5 % más bajo, mínimo, CPU y RAM libre), `samples.csv` (una muestra por segundo) y una copia de `RPCS3.log`.

**Escena de prueba:** siempre la misma, por ejemplo cargar la misma partida guardada y jugar el mismo tramo durante los 120 s. Si cambia la escena, los números no son comparables.

## Perfiles

Se generan con `python3 make_profiles.py` a partir de `profiles/base_BCES00510.yml`, que es la configuración que tenía el juego el 2026-10-20.

| Versión | Cambio respecto a la anterior |
|---|---|
| v0-actual | Ninguno: es la línea base. |
| v1-spu-mega | `SPU Block Size`: Safe → Mega |
| v2-spu-loop | `SPU loop detection`: activado |
| v3-720p | Resolución 720x480 → 1280x720 (la original de PS3) |

Candidatos para las siguientes versiones, según lo que digan las mediciones: planificador de hilos de RPCS3, RSX multihilo apagado, número de hilos SPURS, GPU Radeon 530 frente a Vega 8, y recuperar efectos visuales (bloom, niebla, corrección de color) manteniendo apagado el MLAA de las SPU, que es el que más CPU consume.

## Resultados

| Versión | FPS promedio | FPS 5 % bajo | CPU media | Nota |
|---|---|---|---|---|
| — | — | — | — | Sin mediciones todavía |
