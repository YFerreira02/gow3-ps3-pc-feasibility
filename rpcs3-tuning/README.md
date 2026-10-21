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

Los resultados quedan en el PC en `results\<perfil>_<fecha>\` (en GitHub se suben solo `summary.txt` y `samples.csv`): `summary.txt` (FPS promedio, FPS de juego real, 5 % más bajo, CPU, velocidad real de la CPU en MHz y RAM libre), `samples.csv` (una muestra por segundo), `system.txt` (cargador, plan de energía y programas que más memoria usan) y una copia de `RPCS3.log`.

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
| v8-rsx1-spurs4 | Desde v5: `Max SPURS Threads` 6 → 4 |
| v9-spu-res-off | Desde v5: `Accurate SPU Reservations` apagado (puede causar cuelgues) |
| v10-spu-giga | Desde v5: `SPU Block Size` Mega → Giga (la primera sesión recompila; mídelo dos veces) |

Después, combinar lo que funcione y recuperar efectos visuales (bloom, niebla, corrección de color) manteniendo apagado el MLAA de las SPU, que es el que más CPU consume.

## Resultados

Las cifras usan solo los segundos de **juego real**: CPU al 70 % o más y menos de 29.5 FPS (hasta v5 el umbral era 85 %; con RSX de un hilo la CPU marca menos y ese umbral descartaba segundos de juego). Los menús, las cargas y los momentos a 60 FPS inflan el promedio bruto y no se cuentan. Desde 2026-10-20 el lanzador calcula esto solo (`gameplay_*` en `summary.txt`).

| Versión | FPS juego (mediana) | FPS juego (promedio) | 5 % más bajo | Segundos de juego | Nota |
|---|---|---|---|---|---|
| v0-actual | 11.7 | 12.3 | 8.9 | 78 de 78 | Línea base, 720x480. CPU al 96 %: el límite es la CPU. |
| v1-spu-mega | 6.8 | 7.0 | 1.7 | 36 de 40 | **No válida.** Cambiar el tamaño de bloque SPU obliga a recompilar la caché SPU, y esta sesión pasó compilando. v2 incluye el mismo cambio y no muestra el problema. |
| v2-spu-loop | 12.2 | 13.5 | 9.2 | 62 de 84 | Igual o apenas mejor que v0, dentro del margen de ruido. |
| v3-720p | 12.6 | 13.3 | 9.5 | 35 de 81 | **1280x720 sin costo medible** frente a 720x480. Gran parte de la sesión fue en menús. Pasa a ser el perfil para jugar (`best.txt`). |
| v4-sched-alt | 12.2 | 13.1 | 9.0 | 62 de 81 | Sin cambio frente a v3: el planificador de hilos no es el problema. |
| v5-rsx-1hilo (1) | 16.8 | 16.2 | 11.9 | 52 de 87 | **Mejora clara.** RSX en un solo hilo deja más CPU a las SPU. Desde aquí: cargador conectado, Windows en "Máximo rendimiento" y apps cerradas. |
| v5-rsx-1hilo (2) | 15.3 | 15.5 | 11.8 | 47 de 87 | Repetición: confirma la mejora. Pasa a ser el perfil para jugar (`best.txt`). |
| v7-radeon530 | 10.7 | 13.5 | 8.2 | 34 de 49 | Peor: la GPU dedicada Radeon 530 rinde menos que la Vega 8. Descartada. |
| v6-spurs4 | 12.7 | 14.2 | 10.4 | 52 de 78 | Igual que v3: limitar SPURS a 4 hilos no ayuda. CPU real en juego: ~2.1 GHz. |
| v8-rsx1-spurs4 | 13.1 | 13.1 | 8.3 | 52 de 70 | Peor que v5: con 4 hilos SPURS se pierde lo ganado. CPU real en juego: **~1.9 GHz**. |
| v5 (3), 15 W con apps abiertas | 13.0 | 13.7 | 6.7 | 69 de 76 | Edge y otras apps abiertas: RAM libre mínima **371 MB**. Muestra que la falta de RAM cuesta FPS y tirones. CPU ~1.8 GHz. |
| **v5 (4), 20 W (UXTU)** | **17.0** | **17.0** | 10.4 | 55 de 80 | Límite de consumo subido a 20 W. CPU en juego **~2.4 GHz** (antes ~1.8–1.9), temperatura máx. 83.4 °C, sin freno térmico. RAM libre mínima 3.8 GB. Mejor resultado hasta ahora. |

**Conclusión hasta ahora:** el mejor resultado es **v5 + límite de consumo de 20 W**: unos **17 FPS** en juego real a 1280x720, con tramos de 19–21. Partimos de 12. Limitar los hilos SPURS no ayuda (v6, v8) y la Radeon 530 empeora (v7). Tener RAM libre importa: con Edge y otras apps abiertas, v5 cae a 13 FPS.

**Límite de consumo (2026-10-21):** HWiNFO mostró que, con 15 W, la CPU pasaba el 91 % del tiempo frenada por el límite STAPM, a 81 °C y sin freno térmico. Con Universal x86 Tuning Utility (perfil `GoW3 20W`) se subió el límite. Con él, el consumo llega a 20 W, la CPU sube de ~1.8 a ~2.4 GHz y la temperatura máxima es de 83.4 °C.

| Ajuste de UXTU | Valor |
|---|---|
| Límite de temperatura de la APU | 85 °C |
| Límite de la temperatura de la piel (STAPM) | 20 W |
| Límite de potencia sostenida (Slow) | 22 W |
| Límite de potencia rápida (Fast) | 25 W |
| Perfil de impulso de AMD | Rendimiento |
| Modo de impulso del procesador | Agresivo |
| Modo de energía de Windows | Mejor rendimiento |
| VRM, límites de frecuencia, PBO, Curve Optimizer, Super Resolución, opciones Radeon | Sin tocar |

No se suben voltajes ni corrientes del VRM. El ajuste se pierde al reiniciar.

**Consejo para medir:** tras cambiar ajustes de SPU, la primera sesión compila caché y sale más lenta. Si un perfil nuevo da cifras muy bajas, repítelo una vez.
