import re, sys
src = open(sys.argv[1]).read()
body = src[src.index('void KeyVault::LoadSelfAPPKeys'):]
body = body[:body.index('\n}\n')]
out = []
for m in re.finditer(r'emplace_back\(0x([0-9A-Fa-f]+),\s*0x[0-9A-Fa-f]+,\s*0x([0-9A-Fa-f]+),\s*KEY_APP,\s*"([0-9A-F]*)",\s*"([0-9A-F]*)"', body):
    ver, rev, erk, riv = m.groups()
    out.append(f"[appldr_{rev}]\ntype=SELF\nrevision={rev}\nself_type=APP\nversion={ver}\nerk={erk}\nriv={riv}\n")
open(sys.argv[2], 'w').write('\n'.join(out))
print(len(out), 'keys')
