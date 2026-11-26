"""Map a MoA data address to PP by finding a MoA code reference (lui+addiu/load) and the aligned PP instruction."""
import sys,struct,difflib,bisect
sys.path.insert(0,__import__('os').path.dirname(__import__('os').path.abspath(__file__))); import mdis as D; from ld import n2
rows=[l.rstrip('\n').split('\t') for l in open(D.W+'pp-disc/maps/moa-to-pp-functions.tsv') if l[0]!='#' and not l.startswith('moa_module')]
pairs=sorted((int(r[1],16),int(r[2]),int(r[4],16),int(r[5])) for r in rows if r[0] in ('FDAT 209','SLUS_010.30') and r[3] in('FDAT 204','SLUS_006.70') and r[4])
starts=[p[0] for p in pairs]
def refs(g,target):
    """find (addr) of instructions whose lui-hi + lo == target, within module/exe"""
    out=[]
    for p,b,skip in D.IMG[g]:
        d=open(p,'rb').read()[skip:]; W=struct.unpack('<%dI'%(len(d)//4),d[:len(d)//4*4])
        hi={}
        for i,w in enumerate(W):
            op=w>>26; rs=(w>>21)&31; rt=(w>>16)&31; imm=w&0xffff; simm=imm-0x10000 if imm&0x8000 else imm
            if op==0xF: hi[rt]=(imm<<16,i); continue
            if op in (9,0x20,0x21,0x23,0x24,0x25,0x28,0x29,0x2b) and rs in hi and i-hi[rs][1]<12:
                if (hi[rs][0]+simm)&0xffffffff==target: out.append(b+4*i)
    return out
def pp_value_at(a):
    """PP instruction address -> computed hi+lo (look back for lui of same reg)"""
    for back in range(1,12):
        w=D.rd32('pp',a); rs=(w>>21)&31; imm=w&0xffff; simm=imm-0x10000 if imm&0x8000 else imm
        w2=D.rd32('pp',a-4*back)
        if w2>>26==0xF and ((w2>>16)&31)==rs: return ((w2&0xffff)<<16)+simm & 0xffffffff
    return None
def mapaddr(t):
    res=[]
    for r in refs('ma',t):
        k=bisect.bisect_right(starts,r)-1
        if k<0: continue
        ma0,na,pp0,nb=pairs[k]
        if r>=ma0+4*na: continue
        A=D.dis('ma',ma0,na); B=D.dis('pp',pp0,nb)
        sm=difflib.SequenceMatcher(None,[n2(x[2]) for x in A],[n2(x[2]) for x in B],autojunk=False)
        idx=(r-ma0)//4
        for op,i1,i2,j1,j2 in sm.get_opcodes():
            if op=='equal' and i1<=idx<i2:
                pa=B[j1+idx-i1][0]; v=pp_value_at(pa)
                if v: res.append(v)
    return res
if __name__=='__main__':
    from collections import Counter
    for t in sys.argv[1:]:
        c=Counter(mapaddr(int(t,16))); print(t,'->',', '.join('%08x x%d'%(a,n) for a,n in c.most_common(4)))
