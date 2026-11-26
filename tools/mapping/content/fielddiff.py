import os
import pickle, struct
res=pickle.load(open(os.path.dirname(os.path.abspath(__file__))+'/tables.pkl','rb'))
MA = open(os.environ.get('ACG_WORK','work')+'/ma-disc/unpacked/GG/COM/FDAT/206.bin','rb').read()
PP = open(os.environ.get('ACG_WORK','work')+'/pp-disc/unpacked/GG/COM/FDAT/201.bin','rb').read()
MAB, PPB = 0x80050DD8, 0x800490D0
F={ # (off,size,name)
'common':[(0x24,4,'price'),(0x28,2,'weight'),(0x2A,2,'drain')],
'head':[(0x2C,2,'AP'),(0x2E,2,'def'),(0x30,2,'edef'),(0x32,1,'comp'),(0x33,1,'map'),(0x34,1,'noise'),(0x35,1,'bio'),(0x36,2,'radar'),(0x38,2,'rrange'),(0x3A,1,'rflag'),(0x3B,1,'rdisp')],
'core':[(0x2C,2,'AP'),(0x2E,2,'def'),(0x30,2,'edef'),(0x32,2,'armload'),(0x34,1,'mgresp'),(0x35,1,'mgang'),(0x36,2,'optslots')],
'legs':[(0x2C,2,'AP'),(0x2E,2,'def'),(0x30,2,'edef'),(0x32,2,'carry'),(0x34,2,'drainstand'),(0x36,2,'speed'),(0x38,2,'turnacc'),(0x3A,1,'turnspd'),(0x3B,1,'+3B'),(0x3C,2,'turnback'),(0x3E,2,'+3E'),(0x40,2,'stab'),(0x42,2,'booster'),(0x44,2,'risedrain'),(0x46,2,'pad')],
'arms':[(0x2C,2,'AP'),(0x2E,2,'def'),(0x30,2,'edef'),(0x32,2,'pad')],
'hand':[],'back':[(0x2C,2,'isradar'),(0x2E,2,'rrange'),(0x30,1,'rflag'),(0x31,1,'rdisp'),(0x32,2,'pad')],
'blade':[(0x2C,2,'drain'),(0x2E,2,'flags'),(0x30,2,'attack'),(0x32,2,'impact')],
'booster':[(0x2C,2,'power'),(0x2E,2,'bdrain')],
'gene':[(0x2C,2,'cap'),(0x2E,2,'redzone')],
'fcs':[(0x2C,1,'maxlock'),(0x2D,1,'area')]+[(0x2E+12*k+2*j,2,'c%d.%s'%(k,n)) for k in range(4) for j,n in enumerate(['range','bw','bh','iw','ih','lock'])]+[(0x5E,2,'pad')],
'option':[(0x2C,0,'')],
}
def val(r,o,s): return int.from_bytes(r[o:o+s],'little')
def nm(r): return r[:18].split(b'\0')[0].decode('latin1').strip()
for c,(ma,pp,s,a,pa) in res.items():
    flds=F['common']+[x for x in F[c] if x[1]]
    covered=set()
    for o,sz,_ in flds: covered.update(range(o,o+sz))
    for k in range(min(len(ma),len(pp))):
        m,p=ma[k],pp[k]
        if nm(m)!=nm(p) or m==p: continue
        d=[]
        for o,sz,n in flds:
            if val(m,o,sz)!=val(p,o,sz): d.append('%s PP %d -> MoA %d'%(n,val(p,o,sz),val(m,o,sz)))
        for o in range(s):
            if o not in covered and m[o]!=p[o]: d.append('+%02X PP %02X MoA %02X'%(o,p[o],m[o]))
        print('%-7s %2d %-14s %s'%(c,k,nm(m),'; '.join(d)))
# weapon detail tables
print('\n== weapon detail (24 B)')
DN=['kind','model','pat','anim','hold','sight','lockc','type','ammo','eshot','aprice','range','nmis','interval','flags','pad13','atk','impact','pad17']
DO=[(0,1),(1,1),(2,1),(3,1),(4,1),(5,1),(6,1),(7,1),(8,2),(10,2),(12,2),(14,2),(16,1),(17,1),(18,1),(19,1),(20,2),(22,1),(23,1)]
def det(b,addr,base,i): o=addr-base+24*i; return b[o:o+24]
tabs=[('arms',0x800D1DE4,0x800B9BDC,12,16,12),('hand',0x800D1F64,0x800B9D5C,0,26,21),('back',0x800D21D4,0x800B9F54,0,60,59)]
backmap=lambda i: i if i<17 else i+1  # PP back id -> MoA id
for c,ma_a,pp_a,first,mn,pn in tabs:
    names_pp=[nm(r) for r in res[c][1]]
    names_ma=[nm(r) for r in res[c][0]]
    for i in range(pn):
        mi = backmap(i) if c=='back' else i
        if c=='back' and i==16: continue
        if names_pp[i+first if c=='arms' else i]=='DUMMY': continue
        if mi>=mn: continue
        p=det(PP,pp_a,PPB,i); m=det(MA,ma_a,MAB,mi)
        if p!=m:
            d=['%s %d->%d'%(DN[j],val(p,o,s),val(m,o,s)) for j,(o,s) in enumerate(DO) if val(p,o,s)!=val(m,o,s)]
            print('%-5s PP%2d MoA%2d %-14s %s'%(c,i,mi,names_pp[i+first if c=='arms' else i],'; '.join(d)))
