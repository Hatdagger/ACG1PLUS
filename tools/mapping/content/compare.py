import os
import struct, json
MA = open(os.environ.get('ACG_WORK','work')+'/ma-disc/unpacked/GG/COM/FDAT/206.bin','rb').read()
PP = open(os.environ.get('ACG_WORK','work')+'/pp-disc/unpacked/GG/COM/FDAT/201.bin','rb').read()
MAB, PPB = 0x80050DD8, 0x800490D0
MAc = [('head',0x800CE690,60,18),('core',0x800CEAC8,56,11),('legs',0x800CED30,72,40),
 ('arms',0x800CF870,52,24),('hand',0x800CFD50,44,26),('back',0x800D01C8,52,60),
 ('blade',0x800D0DF8,52,6),('booster',0x800D0F30,48,14),('fcs',0x800D11D0,96,16),
 ('gene',0x800D17D0,48,15),('option',0x800D1AA0,44,13)]
PPs = [0x800B64FC,0x800B6934,0x800B6B9C,0x800B76DC,0x800B7BBC,0x800B7F58,0x800B8B54,0x800B8D28,0x800B8FC8,0x800B95C8,0x800B9898,0x800B9BDC]
def nm(r): return r[:18].split(b'\0')[0].decode('latin1').strip()
res={}
for i,(c,a,s,n) in enumerate(MAc):
    pa=PPs[i]; pn=(PPs[i+1]-pa)//s
    ma=[MA[a-MAB+k*s:a-MAB+(k+1)*s] for k in range(n)]
    pp=[PP[pa-PPB+k*s:pa-PPB+(k+1)*s] for k in range(pn)]
    res[c]=(ma,pp,s,a,pa)
import pickle; pickle.dump(res,open(os.path.dirname(os.path.abspath(__file__))+'/tables.pkl','wb'))
if __name__=='__main__':
  for c,(ma,pp,s,a,pa) in res.items():
    print('== %s  MoA %08X x%d   PP %08X x%d  stride %d'%(c,a,len(ma),pa,len(pp),s))
    mn=[nm(r) for r in ma]; pn=[nm(r) for r in pp]
    for k in range(max(len(mn),len(pn))):
        m=mn[k] if k<len(mn) else ''; p=pn[k] if k<len(pn) else ''
        same=''
        if k<len(ma) and k<len(pp):
            same = 'same' if ma[k]==pp[k] else ('stats differ' if m==p else '')
        print('  %2d  %-18s %-18s %s'%(k,m,p,same))
