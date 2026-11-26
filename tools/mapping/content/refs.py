import os
import struct, sys, collections
# find addresses built by lui/addiu (or lui + lw/lh/lbu offset) in a code blob
def refs(blob, base, lo_lim, hi_lim):
    n=len(blob)//4
    w=struct.unpack('<%dI'%n, blob[:n*4])
    res=collections.Counter(); where=collections.defaultdict(list)
    for i,x in enumerate(w):
        if x>>26==0x0F:  # lui
            rt=(x>>16)&31; hi=x&0xFFFF
            for j in range(i+1,min(i+12,n)):
                y=w[j]; op=y>>26; rs=(y>>21)&31
                if rs==rt and op in (0x09,0x20,0x21,0x23,0x24,0x25,0x28,0x29,0x2B,0x22,0x26):
                    imm=y&0xFFFF; imm=imm-0x10000 if imm&0x8000 else imm
                    a=((hi<<16)+imm)&0xFFFFFFFF
                    if lo_lim<=a<hi_lim: res[a]+=1; where[a].append(base+4*i)
                if (y>>26)==0x0F and ((y>>16)&31)==rt: break
    return res, where
if __name__=='__main__':
    f,base,lo,hi=sys.argv[1],int(sys.argv[2],16),int(sys.argv[3],16),int(sys.argv[4],16)
    r,wh=refs(open(f,'rb').read(),base,lo,hi)
    for a in sorted(r): print('%08X %3d  %s'%(a,r[a],' '.join('%08X'%x for x in wh[a][:3])))
