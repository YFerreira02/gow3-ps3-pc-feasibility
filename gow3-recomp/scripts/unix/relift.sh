#!/bin/sh
# Traduce el EBOOT descifrado a C++ (PPU) y las 8 SPU incrustadas.
# Uso: KIT=../ps3kit ELF=../work/EBOOT.elf scripts/unix/relift.sh
set -e
here=$(cd "$(dirname "$0")/../.." && pwd)
KIT=${KIT:-$here/../ps3kit}; ELF=${ELF:-$here/../work/EBOOT.elf}
mkdir -p "$here/meta/spu" "$here/src/recomp" "$here/src/spu"
python3 "$KIT/tools/ppu_loader.py" "$ELF" -o "$here/meta"
# .text + .lib.stub terminan en 0x3CA804; TOC de módulo 0x52D6C8 (BCES00510 v1.00)
python3 "$KIT/tools/ppu_lifter.py" "$ELF" --functions "$here/meta/EBOOT.functions.json" \
    --hle-stubs "$here/meta/EBOOT.imports.json" --toc 0x52D6C8 --code-end 0x3CA804 \
    --chunk-lines 150000 -o "$here/src/recomp"
python3 "$KIT/tools/extract_spu_images.py" "$ELF" -o "$here/meta/spu"
# Política Wws (SPURS workload 0): código SPU crudo en 0x49FF80, 12032 bytes, carga en LS 0xA00
python3 - "$ELF" "$here/meta/wws_pm.bin" <<'PY'
import sys; d=open(sys.argv[1],'rb').read(); o=0x49FF80-0x10000; open(sys.argv[2],'wb').write(d[o:o+12032])
PY
python3 "$KIT/tools/wrap_spu_elf.py" "$here/meta/wws_pm.bin" --base 0xA00 --entry 0xA00 \
    --out "$here/meta/spu/spu_wws_pm_at_0049FF80.elf"
python3 "$KIT/tools/build_spu_workloads.py" --images "$here/meta/spu" --lifted "$here/src/spu" \
    --out "$here/src/spu/spu_workloads.c" --register-fn gow3_spu_register_all --constructor \
    --title gow3 --relift
