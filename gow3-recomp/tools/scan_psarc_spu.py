"""Busca código SPU en un PSARC: ELF de SPU (EM_SPU=23) y módulos de trabajo
Wws (cabecera ila..; +0x14 = 0xC0DEC0DE). Escribe un resumen y, con --out, los
binarios encontrados (nombre por sha1). No sube nada: los datos son del juego."""
import sys, hashlib, struct, os, collections, argparse
sys.path.insert(0, '/tmp/gow-ascension-pc/tools')
from psarc_replace import Psarc

ap = argparse.ArgumentParser(); ap.add_argument('psarc', nargs='+'); ap.add_argument('--out')
a = ap.parse_args()
if a.out: os.makedirs(a.out, exist_ok=True)
seen = {}
ext = collections.Counter()
for path in a.psarc:
    p = Psarc(path)
    for i in range(1, p.n):
        name = p.names[i - 1] if i - 1 < len(p.names) else f'#{i}'
        ext[os.path.splitext(name)[1].lower()] += 1
        try: d = p.read(i)
        except Exception as e: print('ERR', name, e); continue
        # SPU ELF
        j = d.find(b'\x7fELF\x01\x02')
        while j >= 0:
            if d[j+0x12:j+0x14] == b'\x00\x17':
                b = d[j:j+0x40000]; h = hashlib.sha1(b[:0x1000]).hexdigest()[:12]
                seen.setdefault(('elf', h), (name, j))
            j = d.find(b'\x7fELF\x01\x02', j + 1)
        # Wws job module: word5 == C0DEC0DE, word0 is an ila (opcode 0x21 in top 7 bits -> 0x42..0x43)
        j = d.find(b'\xC0\xDE\xC0\xDE')
        while j >= 0:
            s = j - 0x20          # 4 ila | entry @0x10 | size @0x14 | 0 | 0 | C0DEC0DE @0x20
            if s >= 0 and (d[s] & 0xFE) == 0x42:
                ent, size = struct.unpack('>II', d[s+0x10:s+0x18])
                if 0 < size <= 0x40000:
                    b = d[s:s+size]; h = hashlib.sha1(b).hexdigest()[:12]
                    if ('job', h) not in seen:
                        seen[('job', h)] = (name, s)
                        if a.out: open(f'{a.out}/jobmod_{h}_e{ent:X}.bin', 'wb').write(b)
            j = d.find(b'\xC0\xDE\xC0\xDE', j + 1)
kinds = collections.Counter(k for k, _ in seen)
print('extensiones:', dict(ext.most_common(15)))
print('encontrado:', dict(kinds))
for (k, h), (n, o) in sorted(seen.items())[:60]:
    print(f'  {k} {h}  en {n} @+0x{o:X}')
