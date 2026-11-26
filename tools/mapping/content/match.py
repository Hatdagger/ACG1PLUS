import os
import sys, random, pickle, collections
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from emu import *
res=pickle.load(open(os.path.dirname(os.path.abspath(__file__))+'/tables.pkl','rb'))
def nm(r): return r[:18].split(b'\0')[0].decode('latin1').strip()
# loadout slot -> category (MoA order)
SLOTS=['core','booster','head','arms','blade','hand','back','back','gene','fcs',None,'legs']
mp=lambda i: i if i<17 else i+1
def same_ids(c):
    ma,pp=res[c][0],res[c][1]; out=[]
    for i,r in enumerate(pp):
        if nm(r) in ('DUMMY',''): continue
        mi = mp(i) if c=='back' else i
        if c=='back' and i==16: continue
        if mi<len(ma) and nm(ma[mi])==nm(r): out.append((i,mi,ma[mi]==r))
    return out
IDS={c:same_ids(c) for c in set(x for x in SLOTS if x)}
PPG,MAG=Game('pp'),Game('moa')
def build(rng, identical_only=True):
    pl,ml=[],[]
    for c in SLOTS:
        if c is None: pl.append(0); ml.append(0); continue
        L=[x for x in IDS[c] if x[2] or not identical_only or c=='fcs']
        if c=='arms': L=[x for x in L if x[0]<12] if rng.random()<0.8 else L
        p,m,_=rng.choice(L); pl.append(p); ml.append(m)
    # arms weapon arms -> no hand/blade? keep anyway
    return pl,ml
def runboth(pl,ml,opts=0):
    a,_=PPG.run(pl,opts); b,_=MAG.run(ml,opts)
    return PPG.block(a), MAG.block(b)
if __name__=='__main__':
    rng=random.Random(1); N=int(sys.argv[1]) if len(sys.argv)>1 else 300
    data=[]
    for k in range(N):
        pl,ml=build(rng, identical_only=True)
        try: data.append((pl,ml)+runboth(pl,ml))
        except Exception as e: print('fail',pl,ml,e)
    pickle.dump(data,open(os.path.dirname(os.path.abspath(__file__))+'/runs.pkl','wb'))
    print(len(data),'runs')
