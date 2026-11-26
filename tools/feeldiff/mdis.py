"""mdis.py — disassemble PP / MoA code by RAM address. Usage:
  python3 -I mdis.py pp|ma ADDR [N|end]      (module chosen by address; N instructions, default = to jr ra)
  python3 -I mdis.py diff MA_ADDR PP_ADDR     side-by-side normalized diff
"""
import sys, struct, difflib, re
from capstone import Cs, CS_ARCH_MIPS, CS_MODE_MIPS32, CS_MODE_LITTLE_ENDIAN
import os
W=os.environ.get('FEELDIFF_WORK','.').rstrip('/')+'/'  # folder holding pp-disc/ and ma-disc/ (extracted work packs)
IMG={
 'pp':[(W+'pp-disc/SLUS_006.70',0x80011000,0x800),(W+'pp-disc/unpacked/GG/COM/FDAT/204.bin',0x800490D0,0)],
 'ma':[(W+'ma-disc/SLUS_010.30',0x80011000,0x800),(W+'ma-disc/unpacked/GG/COM/FDAT/209.bin',0x80050DD8,0)],
}
MOD={'pp':{'vs':204,'arena':205,'menu':201,'mis':202,'rep':206},'ma':{'vs':209,'arena':210,'menu':206,'mis':207,'rep':211}}
BASE={'pp':0x800490D0,'ma':0x80050DD8}
def setmod(g,m):
    n=MOD[g][m]; d='pp-disc' if g=='pp' else 'ma-disc'
    IMG[g][1]=(W+d+'/unpacked/GG/COM/FDAT/%d.bin'%n,BASE[g],0)
_cache={}
def blob(g,addr):
    for p,b,skip in reversed(IMG[g]):
        if p not in _cache: _cache[p]=open(p,'rb').read()[skip:]
        d=_cache[p]
        if b<=addr<b+len(d): return d,b
    raise KeyError(hex(addr))
md=Cs(CS_ARCH_MIPS,CS_MODE_MIPS32|CS_MODE_LITTLE_ENDIAN)
def rd32(g,a):
    d,b=blob(g,a); return struct.unpack_from('<I',d,a-b)[0]
def rd16(g,a,s=True):
    d,b=blob(g,a); return struct.unpack_from('<h' if s else '<H',d,a-b)[0]
def dis(g,addr,n=None):
    d,b=blob(g,addr); off=addr-b; out=[]; cnt=0; stop=None
    while off<len(d):
        w=struct.unpack_from('<I',d,off)[0]
        ins=list(md.disasm(d[off:off+4],b+off))
        txt=(ins[0].mnemonic+' '+ins[0].op_str) if ins else '.word 0x%08x'%w
        out.append((b+off,w,txt)); cnt+=1; off+=4
        if stop is not None and cnt>=stop: break
        if n is not None and cnt>=n: break
        if n is None and w==0x03E00008: stop=cnt+1
    return out
def norm(t):
    t=re.sub(r'0x80[0-9a-f]{6}','ADDR',t)
    return t
if __name__=='__main__':
    a=sys.argv
    if a[1]=='diff':
        A=dis('ma',int(a[2],16), int(a[4]) if len(a)>4 else None); B=dis('pp',int(a[3],16), int(a[5]) if len(a)>5 else None)
        sa=[norm(x[2]) for x in A]; sb=[norm(x[2]) for x in B]
        sm=difflib.SequenceMatcher(None,sa,sb,autojunk=False)
        print('ratio %.3f  ma %d  pp %d'%(sm.ratio(),len(A),len(B)))
        for op,i1,i2,j1,j2 in sm.get_opcodes():
            if op=='equal':
                for k in range(i2-i1):
                    x=A[i1+k];y=B[j1+k]
                    m='  ' if x[2]==y[2] else '~ '
                    print('%s%08x %-38s| %08x %s'%(m,x[0],x[2],y[0],y[2]))
            else:
                L=max(i2-i1,j2-j1)
                for k in range(L):
                    x=A[i1+k] if i1+k<i2 else None; y=B[j1+k] if j1+k<j2 else None
                    print('! %-47s| %s'%(('%08x %s'%(x[0],x[2])) if x else '', ('%08x %s'%(y[0],y[2])) if y else ''))
    else:
        g=a[1]; n=None
        if len(a)>3: n=int(a[3])
        for ad,w,t in dis(g,int(a[2],16),n): print('%08x %s'%(ad,t))
