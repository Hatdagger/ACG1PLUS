import sys,re,difflib
sys.path.insert(0,__import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import mdis as D
MEM=re.compile(r'(-?0x[0-9a-f]+|-?\d+)\((\$\w+)\)')
def n_struct(t):
    t=re.sub(r'0x80[0-9a-f]{6}','A',t)
    t=MEM.sub(r'O(R)',t)
    t=re.sub(r'\$\w+','R',t)
    t=re.sub(r', (-?0x[0-9a-f]+|-?\d+)$',', I',t)
    t=re.sub(r'\bnop\b ?','',t).strip()
    return t
def keep(x,a,b):
    a=int(a,16) if 'x' in a else int(a); b=int(b,16) if 'x' in b else int(b)
    m=x.split()[0]
    if '$sp' in x: return False
    if m in('addiu','ori','addi'):
        if '$zero' not in x and abs(a)>0x400: return False
        if '$zero' not in x and abs(a-b) in (4,8,9,0x10): return False
    return True
def body(g,a,n): return [x for x in D.dis(g,a,n) if x[2].strip()!='nop']
rows=[l.rstrip('\n').split('\t') for l in open(D.W+'pp-disc/maps/moa-to-pp-functions.tsv') if not l.startswith('#') and not l.startswith('moa_module')]
mod=sys.argv[1] if len(sys.argv)>1 else 'FDAT 209'
for r in rows:
    if r[0]!=mod or r[6] not in('changed','same shape, numbers or field positions differ'): continue
    ma,pp=int(r[1],16),int(r[4],16)
    A=body('ma',ma,int(r[2])); B=body('pp',pp,int(r[5]))
    sa=[n_struct(x[2]) for x in A]; sb=[n_struct(x[2]) for x in B]
    sm=difflib.SequenceMatcher(None,sa,sb,autojunk=False)
    # constants: compare non-memory immediates in equal blocks
    consts=[]
    for op,i1,i2,j1,j2 in sm.get_opcodes():
        if op=='equal':
            for k in range(i2-i1):
                x=A[i1+k][2];y=B[j1+k][2]
                if 'lui' in x or 'j ' in x[:2] or x.startswith('jal') or x.startswith('b'): continue
                ix=re.findall(r', (-?0x[0-9a-f]+|-?\d+)$',MEM.sub('',x)); iy=re.findall(r', (-?0x[0-9a-f]+|-?\d+)$',MEM.sub('',y))
                if ix and iy and ix!=iy and keep(x,ix[0],iy[0]): consts.append('%08x %s -> %08x %s'%(A[i1+k][0],x,B[j1+k][0],y))
    diffn=sum(max(i2-i1,j2-j1) for op,i1,i2,j1,j2 in sm.get_opcodes() if op!='equal')
    print('%s %s->%s ratio %.3f logicdiff %d consts %d'%(r[0],r[1],r[4],sm.ratio(),diffn,len(consts)))
    for c in consts: print('     C',c)
