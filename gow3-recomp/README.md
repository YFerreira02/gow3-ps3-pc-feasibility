# gow3-recomp — God of War III (PS3) recompilado para PC

Nuevo enfoque del proyecto, desde el 2026-10-21. Se basa en el método de
[Ascension](https://github.com/arnaldolima1/gow-ascension-pc), que hace lo mismo con
*God of War: Ascension*.

No es un emulador. El código del juego (PPU y SPU) se traduce a C++ antes de jugar y
se compila como un programa nativo de Windows. La base es el runtime
[ps3recomp](https://github.com/sp00nznet/ps3recomp).

> **Estado: fase 0 (preparación).** Todavía no hay nada jugable. Mientras tanto, para
> jugar sigue valiendo la ruta RPCS3 de `rpcs3-tuning/` (17 FPS en el portátil).

> **No incluye datos del juego.** Hace falta tu propia copia europea **BCES00510**. El
> ejecutable descifrado, los archivos del disco y todo lo que se genere a partir de
> ellos quedan en `work/` y nunca se suben al repositorio.

## Por qué Ascension sirve de modelo

Ascension y God of War III son del mismo estudio (Santa Monica) y usan generaciones del
mismo motor. Comparten las piezas difíciles que Ascension ya tuvo que resolver:

- SPURS para repartir trabajo entre las SPU.
- Edge Zlib para descomprimir los archivos del juego.
- Módulos de trabajo SPU que se cargan sobre la marcha.
- Sincronización entre la GPU (RSX) y las SPU.
- Postproceso en las SPU. En God of War III incluye el MLAA.

Ascension llegó del arranque al primer nivel en una semana, y el método está
documentado en sus hitos. La mayoría de sus correcciones genéricas ya están en
ps3recomp v0.17.

## Estructura

```
gow3/
  ps3kit/       ps3recomp v0.17 (a6d76c8) + parches de patches/
  gow3-recomp/  esta carpeta
  work/         disco, EBOOT descifrado y código generado (nunca se sube)
```

- `patches/` contiene 9 parches de Ascension que aplican limpio sobre ps3recomp v0.17:
  - Entradas SPURS de segunda generación.
  - Tabla de offsets de E/S de GCM.
  - Bitsets de taskset con bloqueo.
  - Texturas X16 y sRGB.
  - `usleep` de alta resolución.

  Los otros 35 parches de Ascension, o ya están incorporados en v0.17, o son propios
  de ese juego. Se traerán uno por uno cuando God of War III tope con el mismo
  problema.
- `scripts/windows/` contiene los scripts para el PC.
- `PLAN.md` detalla las fases, qué se reutiliza de Ascension y qué esperar de
  rendimiento.

## Empezar (en el PC)

1. Descifra el EBOOT con RPCS3, como se explica en `PLAN.md`, fase 1.
2. Prepara el kit y haz el triaje:
   ```powershell
   .\scripts\windows\setup.ps1
   .\scripts\windows\triage.ps1 -Eboot "C:\ruta\a\EBOOT.elf"
   ```
3. El resultado queda en `work\triage\`. Con eso se decide cuánto trabajo es.

## Créditos

- [ps3recomp](https://github.com/sp00nznet/ps3recomp), de sp00nznet y colaboradores.
- [Ascension](https://github.com/arnaldolima1/gow-ascension-pc), de Arnaldo Lima (MIT).
  Los parches de `patches/` vienen de ahí.
- God of War es una marca registrada de Sony Interactive Entertainment. Este es un
  proyecto de fans, sin relación con Sony ni con Santa Monica Studio.

## Herramientas de descifrado (`tools/`)
- `decrypt_self.py` (de Ascension y Twisted Metal, MIT): convierte el SELF en ELF.
- `kv2scetool.py`: pasa las claves APP del `key_vault.cpp` de RPCS3 al formato de
  scetool. El archivo de claves se genera en local y no se sube.
- `aes_shim_Crypto/`: AES compatible con pycryptodome, montado sobre `cryptography`,
  para cuando pycryptodome no se puede instalar.
