import sys,struct
sys.path.insert(0,__import__('os').path.dirname(__import__('os').path.abspath(__file__))); import mdis as D
def callers(g,target):
    out=[]
    for p,b,skip in D.IMG[g]:
        d=open(p,'rb').read()[skip:]
        for o in range(0,len(d)-3,4):
            w=struct.unpack_from('<I',d,o)[0]
            if (w>>26)==3 and ((w&0x3ffffff)<<2)|0x80000000==target: out.append(b+o)
    return out
def wordrefs(g,val):
    out=[]
    for p,b,skip in D.IMG[g]:
        d=open(p,'rb').read()[skip:]
        for o in range(0,len(d)-3,4):
            if struct.unpack_from('<I',d,o)[0]==val: out.append(b+o)
    return out
if __name__=='__main__':
    g=sys.argv[1]
    for t in sys.argv[2:]:
        t=int(t,16); print(g,hex(t),'calls from',' '.join('%08x'%x for x in callers(g,t)),'| words at',' '.join('%08x'%x for x in wordrefs(g,t)))
