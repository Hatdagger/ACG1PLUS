import sys,struct,re
sys.path.insert(0,__import__('os').path.dirname(__import__('os').path.abspath(__file__))); import mdis as D
g=sys.argv[1]; pat=re.compile(sys.argv[2]); lo=int(sys.argv[3],16) if len(sys.argv)>3 else 0; hi=int(sys.argv[4],16) if len(sys.argv)>4 else 0xffffffff
for p,b,skip in D.IMG[g]:
    d=open(p,'rb').read()[skip:]
    for o in range(0,len(d)-3,4):
        a=b+o
        if not lo<=a<hi: continue
        ins=list(D.md.disasm(d[o:o+4],a))
        if ins:
            t=ins[0].mnemonic+' '+ins[0].op_str
            if pat.search(t): print('%08x %s'%(a,t))
