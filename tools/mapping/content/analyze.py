import os
import pickle, sys
data=pickle.load(open(sys.argv[1] if len(sys.argv)>1 else os.path.dirname(os.path.abspath(__file__))+'/runs.pkl','rb'))
PPN, MAN = 0x13E+2, 0x144
pc=[tuple(d[2][i] for d in data) for i in range(0x150)]
mc=[tuple(d[3][i] for d in data) for i in range(0x150)]
N=len(data)
rows=[]
for j in range(MAN):
    col=mc[j]; const=len(set(col))==1
    best=[]
    for i in range(0x150):
        eq=sum(1 for a,b in zip(pc[i],col) if a==b)
        best.append((eq,i))
    best.sort(reverse=True)
    top=[i for eq,i in best if eq==best[0][0]]
    rows.append((j,const,col[0] if const else None,best[0][0],top))
shift=lambda j,i:i-j
for j,const,cv,eq,top in rows:
    if const:
        print('MoA +%03X const %3d   PP same at %s'%(j,cv,'' if pc[j][0]==cv else 'NOT +same' ) + (' (PP +%03X=%d)'%(j,pc[j][0])))
    else:
        near=[i for i in top if abs(i-j)<=16]
        print('MoA +%03X var  match %3d/%d  PP %s'%(j,eq,N,' '.join('+%03X'%i for i in (near or top)[:4])))
