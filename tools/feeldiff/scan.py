import sys,difflib
sys.path.insert(0,__import__('os').path.dirname(__import__('os').path.abspath(__file__))); import mdis as D; from ld import n2
rows=[l.rstrip('\n').split('\t') for l in open(D.W+'pp-disc/maps/moa-to-pp-functions.tsv') if l[0]!='#' and not l.startswith('moa_module')]
mod=sys.argv[1]
for r in rows:
    if r[0]!=mod or r[6]=='identical' or r[3]=='' : continue
    try:
        A=[x for x in D.dis('ma',int(r[1],16),int(r[2])) if x[2].strip()!='nop']; B=[x for x in D.dis('pp',int(r[4],16),int(r[5])) if x[2].strip()!='nop']
    except Exception as e: continue
    sm=difflib.SequenceMatcher(None,[n2(x[2]) for x in A],[n2(x[2]) for x in B],autojunk=False)
    d=sum(max(i2-i1,j2-j1) for op,i1,i2,j1,j2 in sm.get_opcodes() if op!='equal')
    if d: print('%s %s %4d %4d diff %3d %.3f'%(r[1],r[4],len(A),len(B),d,sm.ratio()))
