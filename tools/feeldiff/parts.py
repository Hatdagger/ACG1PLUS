import struct,re,sys,os
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__))); import mdis as D
ma=open(D.W+'ma-disc/unpacked/GG/COM/FDAT/206.bin','rb').read(); MB=0x80050DD8
pp=open(D.W+'pp-disc/unpacked/GG/COM/FDAT/201.bin','rb').read(); PB=0x800490D0
CAT=[('head',0x800CE690,60,18),('core',0x800CEAC8,56,11),('legs',0x800CED30,72,40),('arms',0x800CF870,52,24),('hand',0x800CFD50,44,26),('back',0x800D01C8,52,60),('blade',0x800D0DF8,52,6),('booster',0x800D0F30,48,14),('fcs',0x800D11D0,96,16),('gen',0x800D17D0,48,15),('opt',0x800D1AA0,44,13)]
def recs():
    for cat,a,st,n in CAT:
        for i in range(n):
            o=a-MB+st*i; r=ma[o:o+st]; name=r[:18].split(b'\0')[0].decode('latin1')
            if not name or name=='DUMMY': continue
            key=r[:36]
            j=pp.find(key)
            yield cat,i,name,st,r,(pp[j:j+st] if j>=0 else None),(PB+j if j>=0 else None)
if __name__=='__main__':
    import sys
    pptab={}
    for cat,i,name,st,a,b,pa in recs():
        if b is None: print('%-7s %2d %-16s MoA only'%(cat,i,name)); continue
        pptab.setdefault(cat,[]).append(pa)
        diffs=[]
        for o in range(36,st,2):
            x=struct.unpack_from('<H',a,o)[0]; y=struct.unpack_from('<H',b,o)[0]
            if x!=y: diffs.append('+0x%02X %d->%d'%(o,y,x))
        if diffs: print('%-7s %2d %-16s PP->MoA: %s'%(cat,i,name,', '.join(diffs)))
    for c,v in pptab.items(): print('PP',c,'first record at %08x'%min(v))
