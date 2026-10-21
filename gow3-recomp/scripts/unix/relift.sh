#!/bin/sh
# Traduce el EBOOT descifrado a C++ (PPU) y las 8 SPU incrustadas.
# Uso: KIT=../ps3kit ELF=../work/EBOOT.elf scripts/unix/relift.sh
# KIT = ps3recomp ff66a35 + patches-v2/gow3-kit-sobre-ff66a35.patch (ver patches-v2/LEEME.md)
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
    --title gow3 --relift \
    --extra-funcs spu_wws_pm_at_0049FF80=0xA2C,0xA68   # manejador de interrupciones de la política Wws

# Módulos de trabajo Wws incrustados en el EBOOT (cabecera: 4 ila | entry @0x10 |
# tamaño @0x14 | 0 | 0 | 0xC0DEC0DE @0x20). Se omiten los 4 que son carga útil de
# los ELF SPU ya traducidos. 20 módulos, imágenes 200..219.
mkdir -p "$here/meta/jobmods"
python3 - "$ELF" "$here/meta/jobmods" <<'PY'
import sys, struct, hashlib
d = open(sys.argv[1], 'rb').read(); out = sys.argv[2]
skip = {0x4a3180, 0x4b5c00, 0x4d8b00, 0x4efa80}
st = []; j = d.find(b'\xC0\xDE\xC0\xDE')
while j >= 0:
    if j >= 0x20: st.append(j - 0x20)
    j = d.find(b'\xC0\xDE\xC0\xDE', j + 1)
st.sort()
for i, s in enumerate(st):
    va = s + 0x10000 if s < 0x4f0000 else s - 0x4f0000 + 0x500000
    if va in skip: continue
    ent, size = struct.unpack('>II', d[s + 0x10:s + 0x18])
    b = d[s:s + min(size, (st[i + 1] if i + 1 < len(st) else len(d)) - s)]
    open(f'{out}/jobmod_{hashlib.sha1(b).hexdigest()[:12]}_e{ent:04X}.bin', 'wb').write(b)
PY
python3 "$KIT/tools/lift_jobmods.py" --jobmods "$here/meta/jobmods" --out "$here/src/spu" \
    --register "$here/src/spu/jobmods_register.c" --symbol-prefix gow3 \
    --extra-funcs-json "$here/tools/jobmods_extra_funcs.json"   # entradas vistas en "[spu-smc] microstep ... ovl=N"
