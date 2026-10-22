# God of War III PC — hoja de ruta técnica

## Objetivo
Investigar una recompilación estática de la edición PS3 de God of War III para Windows x64, inspirada en la arquitectura modular de gow-ascension-pc y basada en una versión fija de ps3recomp. No afirmar jugabilidad ni metas de FPS hasta medir escenas reales.

## Repositorios de referencia
- Ascension: https://github.com/arnaldolima1/gow-ascension-pc
- ps3recomp: https://github.com/sp00nznet/ps3recomp

## Principios
1. Separar runtime genérico, código generado por juego, adaptaciones específicas, herramientas y datos locales.
2. Fijar commits exactos de ps3recomp y registrar cualquier parche portado.
3. Mantener EBOOT, ISO, firmware, claves, código generado a partir del binario y capturas de gameplay fuera de Git.
4. No copiar automáticamente todo Ascension. Revisar cada parche según la versión del toolkit, las diferencias del juego y la licencia.
5. Nunca declarar una etapa terminada solo porque compila. Registrar pruebas y evidencia.
6. Optimizar después de obtener perfiles fiables: uso de CPU por hilo, tiempo SPU, GPU por motor, frame time y puntos de espera.

## Etapas y criterios de salida

### Fase 0 — fundación
- Crear el repo, .gitignore robusto, CMake, clang-cl, Ninja y Python.
- Fijar versión de ps3recomp.
- Documentar la versión/Title ID del juego que se posee.
- Resultado: build limpio del runtime y pipeline de análisis reproducible.

### Fase 1 — inspección binaria
- Analizar ELF/SELF, segmentos, punto de entrada, OPD, funciones, TOC e importaciones.
- Inventariar PRX, programas SPU embebidos y módulos que se cargan dinámicamente.
- No subir datos del juego.
- Resultado: análisis en JSON + informe de compatibilidad y riesgos.

### Fase 2 — PPU lift/build/boot
- Generar JSON de funciones/importaciones.
- Lifter PPU incluyendo stubs HLE, generar tabla NID y compilar.
- Ejecutar únicamente sobre los archivos locales legítimos del usuario.
- Resultado: primer arranque medible, con primer fallo no soportado bien registrado.

### Fase 3 — HLE y servicios
- Priorizar imports/syscalls observados durante el arranque.
- Implementar comportamiento verificable; stubs triviales solamente si la semántica está confirmada.
- Resultado: pasar inicialización, filesystem y carga de módulos requeridos.

### Fase 4 — SPU/SPURS
- Inventariar imágenes SPU, overlays, tasksets y job chains.
- Validar ISA, local store, DMA/mailbox/eventos y dispatch.
- Reutilizar y adaptar parches por uno, acompañados de test y rollback.
- Resultado: jobs de prueba con estado y resultados reproducibles.

### Fase 5 — RSX/backend gráfico
- Elegir una ruta de referencia compatible con licencia/dependencias.
- Validar inicialización, command buffer, recursos, shaders, sincronización y presentación.
- GPU smoke tests no cuentan como prueba del juego.
- Resultado: escenas del juego representadas de forma estable.

### Fase 6 — audio, vídeo, entrada y juego
- Implementar el mínimo de audio/vídeo/input necesario.
- Avanzar por checkpoints: boot, intro, menú, carga y primer nivel.
- Resultado: secuencia interactiva estable y reproducible.

### Fase 7 — rendimiento
- Medir CPU por hilo, tiempo en PPU, carga de SPU, uso de GPU por motor, frame time, RAM y temperatura.
- Optimizar hot paths con replay de trabajos y comparación byte a byte cuando proceda.
- Evaluar resolución y 50–60 FPS solo con escenas comparables de gameplay.

### Fase 8 — UX y empaquetado
- Solo después de la estabilidad: launcher, settings, input mapping, logs, empaquetado y docs.

## Estructura inicial sugerida
- docs/: arquitectura, roadmap, compatibility matrix, ADRs, estado de hitos
- tools/analyze/: inspección y reportes
- tools/build/: configuración y scripts reproducibles
- tools/benchmark/: parseo de logs y métricas
- src/game/: defaults y adaptaciones específicas del título
- src/hle/: extensiones específicas solo si no corresponden al toolkit común
- generated/: código generado local, ignorado en Git
- work/: juego y artefactos locales, ignorados en Git
- tests/unit, tests/integration, tests/regression

La estructura se materializará según el código que realmente se necesite, no con módulos vacíos por adelantado.

## Diario por tarea
Por cada cambio registrar:
- commit base y archivos tocados
- problema observable
- hipótesis, evidencia y alternativas descartadas
- prueba mínima de reproducción
- resultado antes/después
- comandos ejecutados
- limitaciones pendientes

## No hacer
- No prometer FPS.
- No ignorar errores HLE o devolver éxito ficticio para forzar el avance.
- No alterar la imagen del juego.
- No aplicar en bloque parches de Ascension sin validar.
- No subir, publicar ni distribuir archivos del juego, claves ni salidas derivadas.
- No reescribir todo el proyecto a la vez.
