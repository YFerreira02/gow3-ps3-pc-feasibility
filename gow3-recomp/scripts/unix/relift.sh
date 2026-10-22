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
# --extra-funcs: manejador de interrupciones de la política Wws (0xA2C, 0xA68)
python3 "$KIT/tools/build_spu_workloads.py" --images "$here/meta/spu" --lifted "$here/src/spu" \
    --out "$here/src/spu/spu_workloads.c" --register-fn gow3_spu_register_all --constructor \
    --title gow3 --relift \
    --extra-funcs spu_wws_pm_at_0049FF80=0xA2C,0xA68 \
    --extra-funcs spu_0004_at_00493100=0x412c,0x5a70,0x9e48,0xa550,0xbf00,0xff98,0x10038,0x10128,0x10208,0x102c8,0x10348,0x10378,0x103e0,0x10458,0x10b70,0x10d54,0x10f80,0x10fc0 \
    --extra-funcs spu_0005_at_004A5B80=0x412c,0xa544,0xa878,0xfd28,0x114b8,0x12138,0x126b8,0x128d8,0x13a60,0x152d0,0x15548,0x15630,0x15708,0x16058,0x16410,0x175b0,0x17de8,0x19658,0x19820,0x1a060,0x1a188,0x1a1a0,0x1a1e8,0x1ec18,0x1ecb8,0x1eda8,0x1ee88,0x1ef48,0x1efc8,0x1eff8,0x1f060,0x1f0d8,0x1f7f0,0x1f9d4,0x1fc00,0x1fc40 \
    --extra-funcs spu_0006_at_004C8A80=0x412c,0x11018,0x110b8,0x111a8,0x11288,0x11348,0x113c8,0x113f8,0x11460,0x114d8,0x12000,0x12040 \
    --extra-funcs spu_0007_at_004DFA00=0x412c,0x4a80,0x4d70,0x7f98,0x8038,0x8128,0x8208,0x82c8,0x8348,0x8378,0x83e0,0x8458,0x8b70,0x8d54,0x8f80,0x8fc0
# Las 4 últimas: entradas de los ELF SPU cuando la política Wws los carga como
# módulos de trabajo (bloque común en base+0x18..0x1040; ver host/gow3_spu_elf_overlays.c).

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
