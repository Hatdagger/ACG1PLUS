import glob,struct,sys,os
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__))); import mdis as D
def ent(g,n,e):
    d={'ma':'ma-disc','pp':'pp-disc'}[g]
    p=glob.glob(D.W+'%s/unpacked/GG/*/PA%02d/%03d.bin'%(d,n,e)); return open(p[0],'rb').read() if p else None
def anims(buf):
    s=buf[4:]
    cnt=struct.unpack_from('<I',s)[0]; out=[]
    for i in range(cnt):
        off=struct.unpack_from('<H',s,12+2*i)[0]+12
        u1,u2,ln,kc=struct.unpack_from('<4H',s,off)
        rot=kc>>15; kc&=0x7fff
        keys=[struct.unpack_from('<Hhhh',s,off+8+8*k) for k in range(kc)]
        out.append(dict(i=i,h=(u1,u2),len=ln,rot=rot,keys=keys,off=off))
    return out
if __name__=='__main__':
    n=int(sys.argv[1])
    A=anims(ent('ma',n,1)); B=anims(ent('pp',n,1))
    print('count',len(A),len(B))
    for i in range(max(len(A),len(B))):
        a=A[i] if i<len(A) else None; b=B[i] if i<len(B) else None
        ka=(a['h'],a['len'],a['keys']) if a else None; kb=(b['h'],b['len'],b['keys']) if b else None
        if ka!=kb: print(i,'MA',ka,'\n   PP',kb)
