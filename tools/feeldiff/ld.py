import sys,re,difflib
sys.path.insert(0,__import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import mdis as D
from deep import n_struct
def n2(t):
    t=n_struct(t); t=re.sub(r'^(lhu|lh|lw|lbu|lb)\b','LD',t); t=re.sub(r'^(sh|sw|sb)\b','ST',t); return t
def run(ma,pp,ctx=1,na=None,nb=None):
    A=[x for x in D.dis('ma',ma,na) if x[2].strip()!='nop']; B=[x for x in D.dis('pp',pp,nb) if x[2].strip()!='nop']
    sm=difflib.SequenceMatcher(None,[n2(x[2]) for x in A],[n2(x[2]) for x in B],autojunk=False)
    print('== MA %08x (%d) PP %08x (%d) ratio %.3f'%(ma,len(A),pp,len(B),sm.ratio()))
    ops=sm.get_opcodes()
    for idx,(op,i1,i2,j1,j2) in enumerate(ops):
        if op=='equal':
            # show context
            if idx>0 and i2-i1>2*ctx:
                for k in range(ctx): print('  %08x %-36s| %08x %s'%(A[i1+k][0],A[i1+k][2],B[j1+k][0],B[j1+k][2]))
                print('  ...')
                for k in range(i2-i1-ctx,i2-i1): print('  %08x %-36s| %08x %s'%(A[i1+k][0],A[i1+k][2],B[j1+k][0],B[j1+k][2]))
            elif idx>0 or i2-i1<=ctx:
                for k in range(i2-i1): print('  %08x %-36s| %08x %s'%(A[i1+k][0],A[i1+k][2],B[j1+k][0],B[j1+k][2]))
            else:
                for k in range(i2-i1-ctx,i2-i1): print('  %08x %-36s| %08x %s'%(A[i1+k][0],A[i1+k][2],B[j1+k][0],B[j1+k][2]))
            continue
        L=max(i2-i1,j2-j1)
        for k in range(L):
            x=A[i1+k] if i1+k<i2 else None; y=B[j1+k] if j1+k<j2 else None
            print('! %-45s| %s'%(('%08x %s'%(x[0],x[2])) if x else '', ('%08x %s'%(y[0],y[2])) if y else ''))
if __name__=='__main__':
    a=sys.argv
    run(int(a[1],16),int(a[2],16),int(a[3]) if len(a)>3 else 1, int(a[4]) if len(a)>4 else None, int(a[5]) if len(a)>5 else None)
