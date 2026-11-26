"""Compare two MIPS code blobs function by function, ignoring where things were placed in memory.
Library + CLI:  python3 -I mipscmp.py A.bin A_base B.bin B_base [--focus hexaddr,...]  (focus = addresses in B)
"""
import struct, sys, difflib, hashlib

def words(path, skip=0):
    b = open(path, 'rb').read()[skip:]
    return list(struct.unpack('<%dI' % (len(b) // 4), b[:len(b) // 4 * 4]))

def funcs(ws):
    """split after each 'jr ra' + delay slot; returns list of (start_index, end_index)"""
    out, s, i, n = [], 0, 0, len(ws)
    while i < n:
        if ws[i] == 0x03E00008:
            e = min(i + 2, n)
            # swallow nop padding
            while e < n and ws[e] == 0:
                e += 1
            out.append((s, e)); s = e; i = e
        else:
            i += 1
    if s < n:
        out.append((s, n))
    return out

def norm(seg, strict):
    res, lui = [], set()
    for w in seg:
        op = w >> 26; rs = (w >> 21) & 31; rt = (w >> 16) & 31
        if op in (2, 3):
            res.append(w & 0xFC000000)
        elif op == 0 or op == 1 or 4 <= op <= 7 or 16 <= op <= 19:
            res.append(w)
            if op == 0:
                lui.discard((w >> 11) & 31)
        elif op == 0xF:
            res.append(w & 0xFFFF0000); lui.add(rt)
        else:
            store = 40 <= op <= 46
            if rs == 29:
                res.append(w)
            elif not strict or rs in lui or rs == 28:
                res.append(w & 0xFFFF0000)
            else:
                res.append(w)
            if not store and not (rs == rt and rt in lui and op in (9, 13)):
                lui.discard(rt)
    return res

def key(seg, strict):
    return hashlib.md5(struct.pack('<%dI' % len(seg), *norm(seg, strict))).digest()

def compare(A, B):
    """A, B = word lists. Returns rows: (kind, a_range or None, b_range or None, ratio)"""
    fa, fb = funcs(A), funcs(B)
    ka = [key(A[s:e], False) for s, e in fa]; kb = [key(B[s:e], False) for s, e in fb]
    sm = difflib.SequenceMatcher(None, ka, kb, autojunk=False)
    rows = []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            for d in range(i2 - i1):
                a, b = fa[i1 + d], fb[j1 + d]
                same = key(A[a[0]:a[1]], True) == key(B[b[0]:b[1]], True)
                rows.append(('same' if same else 'values', a, b, 1.0))
        else:
            ua, ub = list(range(i1, i2)), list(range(j1, j2))
            # greedy pairing by similarity, order-preserving
            la = [norm(A[fa[i][0]:fa[i][1]], False) for i in ua]
            lb = [norm(B[fb[j][0]:fb[j][1]], False) for j in ub]
            pairs, lastj = [], -1
            for x, i in enumerate(ua):
                best, bj = 0.0, None
                for y in range(lastj + 1, len(ub)):
                    if abs(len(la[x]) - len(lb[y])) > 0.6 * max(len(la[x]), len(lb[y]), 1):
                        continue
                    r = difflib.SequenceMatcher(None, la[x], lb[y], autojunk=False).ratio()
                    if r > best:
                        best, bj = r, y
                if bj is not None and best >= 0.5:
                    pairs.append((x, bj, best)); lastj = bj
            pa = {x for x, _, _ in pairs}; pb = {y for _, y, _ in pairs}
            for x, y, r in pairs:
                rows.append(('changed', fa[ua[x]], fb[ub[y]], r))
            for x, i in enumerate(ua):
                if x not in pa: rows.append(('only_a', fa[i], None, 0.0))
            for y, j in enumerate(ub):
                if y not in pb: rows.append(('only_b', None, fb[j], 0.0))
    return rows

def summary(rows):
    t = {}
    for k, a, b, r in rows:
        n = ((a[1] - a[0]) if a else 0, (b[1] - b[0]) if b else 0)
        c = t.setdefault(k, [0, 0, 0]); c[0] += 1; c[1] += n[0] * 4; c[2] += n[1] * 4
    return t
