# Pilas de cada hilo de un minidump de Gow3Recomp.exe (barrido de pila, solo
# direcciones de retorno válidas). Uso: python3 pilas_minidump.py volcado.dmp [marcos]
# Necesita /tmp/claude-0/nm.txt = llvm-nm del .exe exacto (líneas t/T ordenadas).
import struct,sys,bisect
exe=open('/home/claude/w/Gow3Recomp_en_pc.exe','rb').read()
# map rva->file offset for .text: parse PE
pe=struct.unpack_from('<I',exe,0x3C)[0]; nsec=struct.unpack_from('<H',exe,pe+6)[0]; opt=struct.unpack_from('<H',exe,pe+20)[0]
secs=[]
for i in range(nsec):
    o=pe+24+opt+40*i; name=exe[o:o+8].rstrip(b'\0'); vs,va,rs,rp=struct.unpack_from('<IIII',exe,o+8); secs.append((name,va,vs,rp))
tx=[s for s in secs if s[0]==b'.text'][0]
def isret(rva):
    if not (tx[1]+6<=rva<tx[1]+tx[2]): return False
    f=rva-tx[1]+tx[3]
    if exe[f-5]==0xE8: return True
    if exe[f-2]==0xFF and (exe[f-1]>>3)&7==2: return True
    if exe[f-3]==0xFF and (exe[f-2]>>3)&7==2: return True
    if exe[f-6]==0xFF and (exe[f-5]>>3)&7==2: return True
    return False
syms=[(int(l.split()[0],16)-0x140000000,l.split()[2]) for l in open('/tmp/claude-0/nm.txt')]
addrs=[a for a,_ in syms]
def sym(r):
    i=bisect.bisect_right(addrs,r)-1; return '%s+0x%X'%(syms[i][1],r-syms[i][0])
d=open(sys.argv[1],'rb').read()
sig,ver,nst,dirrva=struct.unpack_from('<IIII',d,0)
st={}
for i in range(nst):
    t,sz,rva=struct.unpack_from('<III',d,dirrva+12*i); st[t]=(sz,rva)
def mstr(rva):
    n=struct.unpack_from('<I',d,rva)[0]; return d[rva+4:rva+4+n].decode('utf-16le')
mods=[]
sz,rva=st[4]; n=struct.unpack_from('<I',d,rva)[0]
for i in range(n):
    o=rva+4+108*i; base,size=struct.unpack_from('<QI',d,o); mods.append((base,size,mstr(struct.unpack_from('<I',d,o+20)[0]).split('\\')[-1]))
eb=[m for m in mods if m[2].lower().startswith('gow3')][0][0]
def mod(a):
    for b,s,nm in mods:
        if b<=a<b+s: return nm,a-b
sz,rva=st[3]; n=struct.unpack_from('<I',d,rva)[0]
for i in range(n):
    o=rva+4+48*i
    tid,_,_,_,teb,ss,ssz,srva,csz,crva=struct.unpack_from('<IIIIQQIIII',d,o)
    rip=struct.unpack_from('<Q',d,crva+0xF8)[0]; rsp=struct.unpack_from('<Q',d,crva+0x98)[0]
    stk=d[srva:srva+ssz]; out=[]
    m=mod(rip); out.append('RIP '+('%s+0x%X'%m if m else hex(rip)) + ((' '+sym(rip-eb)) if m and m[0]==mods[0][2] else ''))
    for k in range(max(0,rsp-ss),len(stk)-7,8):
        v=struct.unpack_from('<Q',stk,k)[0]
        if eb<=v<eb+0x8000000 and isret(v-eb): out.append(sym(v-eb))
        if len(out)>=int(sys.argv[2]): break
    print('THREAD',tid,' <- '.join(out))
