import os
import sys,random,pickle; sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import match as M
from emu import P
res=M.res
PPM=open(os.environ.get('ACG_WORK','work')+'/pp-disc/unpacked/GG/COM/FDAT/201.bin','rb').read(); PPB=0x800490D0
def ppdet(addr,i): o=addr-PPB+24*i; return PPM[o:o+24]
def setup(mu):
    for c,(ma,pp,s,a,pa) in res.items():
        for i,r in enumerate(pp):
            mi = M.mp(i) if c=='back' else i
            if c=='back' and i==16: continue
            if mi<len(ma) and M.nm(ma[mi])==M.nm(r): mu.mem_write(P(a+s*mi),r)
    for ma_a,pp_a,n,isback in [(0x800D1DE4,0x800B9BDC,16,0),(0x800D1F64,0x800B9D5C,21,0),(0x800D21D4,0x800B9F54,59,1)]:
        for i in range(n):
            if isback and i==16: continue
            mi=M.mp(i) if isback else i
            d=bytearray(ppdet(pp_a,i)); d[7]=255 if False else d[7]
            mu.mem_write(P(ma_a+24*mi),bytes(d))
if __name__=='__main__':
    rng=random.Random(3); data=[]
    for k in range(int(sys.argv[1])):
        pl,ml=M.build(rng,identical_only=False)
        a,_=M.PPG.run(pl); b,_=M.MAG.run(ml,setup=setup)
        data.append((pl,ml,M.PPG.block(a),M.MAG.block(b)))
    pickle.dump(data,open(sys.argv[2],'wb'))
