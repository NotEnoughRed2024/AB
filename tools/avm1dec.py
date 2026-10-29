"""Rough AVM1 decompiler: turns the stack code of avm1dis.py-style bytecode into readable pseudo-code
(expressions rebuilt from the stack, branches left as gotos). Good enough to read Haxe-generated code.

usage: python3 avm1dec.py file.swf [start_offset end_offset] > out.txt
"""
import struct
import sys
import zlib

sys.setrecursionlimit(10000)


def cstr(d, p):
    e = d.index(b'\0', p)
    return d[p:e].decode('latin-1'), e + 1


BIN = {0x47: '+', 0x0B: '-', 0x0C: '*', 0x0D: '/', 0x3F: '%', 0x48: '<', 0x67: '>', 0x49: '==', 0x66: '===',
       0x60: '&', 0x61: '|', 0x62: '^', 0x63: '<<', 0x64: '>>', 0x65: '>>>', 0x0A: '+', 0x0E: '==', 0x0F: '<',
       0x10: '&&', 0x11: '||', 0x21: '..', 0x29: 'lt$', 0x68: 'gt$', 0x13: 'eq$', 0x54: 'instanceof'}


class Dec:
    def __init__(self, d):
        self.d = d
        self.out = []

    def run(self, start, end, pool, indent, regnames):
        d = self.d
        st = []
        pad = '    ' * indent
        p = start
        labels = set()
        # first pass: collect jump targets
        q = start
        while q < end:
            op = d[q]
            q2 = q + 1
            ln = 0
            if op >= 0x80:
                ln = struct.unpack_from('<H', d, q2)[0]
                q2 += 2
            if op in (0x99, 0x9D):
                labels.add(q2 + ln + struct.unpack_from('<h', d, q2)[0])
            if op in (0x8E, 0x9B):
                body = d[q2:q2 + ln]
                size = struct.unpack_from('<H', body, ln - 2)[0]
                q = q2 + ln + size
                continue
            q = q2 + ln

        def emit(s):
            self.out.append(pad + s)

        def pop():
            return st.pop() if st else '<?>'

        def reg(n):
            return regnames.get(n, 'r%d' % n)

        while p < end:
            if p in labels:
                if st:
                    emit('// stack: ' + ', '.join(st))
                emit('L%d:' % p)
            op = d[p]
            at = p
            p += 1
            ln = 0
            if op >= 0x80:
                ln = struct.unpack_from('<H', d, p)[0]
                p += 2
            body = d[p:p + ln]
            nxt = p + ln
            if op == 0x88:
                n = struct.unpack_from('<H', body, 0)[0]
                q = 2
                pool[:] = []
                for _ in range(n):
                    s, q = cstr(body, q)
                    pool.append(s)
            elif op == 0x96:
                q = 0
                while q < ln:
                    t = body[q]
                    q += 1
                    if t == 0:
                        s, q = cstr(body, q)
                        st.append(repr(s))
                    elif t == 1:
                        st.append(repr(struct.unpack_from('<f', body, q)[0]))
                        q += 4
                    elif t == 2:
                        st.append('null')
                    elif t == 3:
                        st.append('undefined')
                    elif t == 4:
                        st.append(reg(body[q]))
                        q += 1
                    elif t == 5:
                        st.append('true' if body[q] else 'false')
                        q += 1
                    elif t == 6:
                        hi, lo = struct.unpack_from('<II', body, q)
                        v = struct.unpack('<d', struct.pack('<II', lo, hi))[0]
                        st.append(repr(int(v)) if v == int(v) else repr(v))
                        q += 8
                    elif t == 7:
                        st.append(str(struct.unpack_from('<i', body, q)[0]))
                        q += 4
                    elif t == 8:
                        st.append(repr(pool[body[q]]))
                        q += 1
                    elif t == 9:
                        st.append(repr(pool[struct.unpack_from('<H', body, q)[0]]))
                        q += 2
            elif op in BIN:
                b = pop()
                a = pop()
                st.append('(%s %s %s)' % (a, BIN[op], b))
            elif op == 0x12:
                st.append('!' + pop())
            elif op == 0x17:
                v = pop()
                if v.startswith('$call'):
                    emit(v[5:] + ';')
                elif '(' in v and v.endswith(')'):
                    emit(v + ';')
            elif op == 0x1C:
                st.append('$' + pop().strip("'"))
            elif op == 0x1D:
                v = pop()
                n = pop()
                emit('$%s = %s;' % (n.strip("'"), v))
            elif op == 0x3C:
                v = pop()
                n = pop()
                emit('var %s = %s;' % (n.strip("'"), v))
            elif op == 0x41:
                emit('var %s;' % pop().strip("'"))
            elif op == 0x4E:
                m = pop()
                o = pop()
                st.append('%s.%s' % (o, m.strip("'")) if m.startswith("'") else '%s[%s]' % (o, m))
            elif op == 0x4F:
                v = pop()
                m = pop()
                o = pop()
                emit('%s.%s = %s;' % (o, m.strip("'"), v) if m.startswith("'") else '%s[%s] = %s;' % (o, m, v))
            elif op == 0x3D:
                f = pop()
                n = pop()
                try:
                    n = int(n)
                except ValueError:
                    n = 0
                args = [pop() for _ in range(n)]
                st.append('%s(%s)' % (f.strip("'"), ', '.join(args)))
            elif op == 0x52:
                m = pop()
                o = pop()
                n = pop()
                try:
                    n = int(n)
                except ValueError:
                    n = 0
                args = [pop() for _ in range(n)]
                if m in ("''", 'undefined'):
                    st.append('%s(%s)' % (o, ', '.join(args)))
                else:
                    st.append('%s.%s(%s)' % (o, m.strip("'"), ', '.join(args)))
            elif op in (0x40, 0x53):
                if op == 0x40:
                    f = pop()
                    o = None
                else:
                    m = pop()
                    o = pop()
                    f = '%s.%s' % (o, m.strip("'"))
                n = pop()
                try:
                    n = int(n)
                except ValueError:
                    n = 0
                args = [pop() for _ in range(n)]
                st.append('new %s(%s)' % (f.strip("'"), ', '.join(args)))
            elif op == 0x42:
                n = pop()
                try:
                    n = int(n)
                except ValueError:
                    n = 0
                st.append('[%s]' % ', '.join(pop() for _ in range(n)))
            elif op == 0x43:
                n = pop()
                try:
                    n = int(n)
                except ValueError:
                    n = 0
                items = []
                for _ in range(n):
                    v = pop()
                    k = pop()
                    items.append('%s: %s' % (k.strip("'"), v))
                st.append('{%s}' % ', '.join(items))
            elif op == 0x87:
                v = st[-1] if st else '<?>'
                r = body[0]
                emit('%s = %s;' % (reg(r), v))
                st[-1:] = [reg(r)] if st else []
            elif op == 0x4C:
                st.append(st[-1] if st else '<?>')
            elif op == 0x4D:
                if len(st) >= 2:
                    st[-1], st[-2] = st[-2], st[-1]
            elif op == 0x99:
                tgt = nxt + struct.unpack_from('<h', body, 0)[0]
                if st:
                    emit('// stack: ' + ', '.join(st))
                emit('goto L%d;' % tgt)
            elif op == 0x9D:
                tgt = nxt + struct.unpack_from('<h', body, 0)[0]
                emit('if %s goto L%d;' % (pop(), tgt))
            elif op == 0x3E:
                emit('return %s;' % pop())
            elif op == 0x2A:
                emit('throw %s;' % pop())
            elif op == 0x50:
                st.append('(%s + 1)' % pop())
            elif op == 0x51:
                st.append('(%s - 1)' % pop())
            elif op == 0x44:
                st.append('typeof(%s)' % pop())
            elif op == 0x4A:
                st.append('Number(%s)' % pop())
            elif op == 0x4B:
                st.append('String(%s)' % pop())
            elif op == 0x18:
                st.append('int(%s)' % pop())
            elif op == 0x3A:
                m = pop()
                o = pop()
                emit('delete %s[%s];' % (o, m))
                st.append('true')
            elif op == 0x3B:
                emit('delete %s;' % pop())
                st.append('true')
            elif op == 0x46 or op == 0x55:
                o = pop()
                st.append('null')
                emit('// enumerate %s (pushes keys)' % o)
            elif op == 0x26:
                emit('trace(%s);' % pop())
            elif op == 0x30:
                st.append('random(%s)' % pop())
            elif op == 0x34:
                st.append('getTimer()')
            elif op == 0x22:
                i = pop()
                t = pop()
                st.append('getProperty(%s, %s)' % (t, i))
            elif op == 0x23:
                v = pop()
                i = pop()
                t = pop()
                emit('setProperty(%s, %s, %s);' % (t, i, v))
            elif op == 0x69:
                sup = pop()
                sub = pop()
                emit('%s extends %s;' % (sub, sup))
            elif op == 0x2B:
                o = pop()
                c = pop()
                st.append('cast(%s, %s)' % (o, c))
            elif op == 0x2C:
                c = pop()
                n = pop()
                try:
                    n = int(n)
                except ValueError:
                    n = 0
                ifs = [pop() for _ in range(n)]
                emit('%s implements %s;' % (c, ifs))
            elif op in (0x8E, 0x9B):
                q = 0
                fname, q = cstr(body, q)
                regmap = {}
                if op == 0x8E:
                    nparams = struct.unpack_from('<H', body, q)[0]
                    q += 2
                    q += 1
                    flags = struct.unpack_from('<H', body, q)[0]
                    q += 2
                    params = []
                    for _ in range(nparams):
                        r = body[q]
                        q += 1
                        pn, q = cstr(body, q)
                        if not pn:
                            pn = 'a%d' % len(params)
                        params.append(pn)
                        if r:
                            regmap[r] = pn
                    r = 1
                    for bit, nm in ((0x01, 'this'), (0x04, 'arguments'), (0x10, 'super'), (0x40, '_root'),
                                    (0x80, '_parent'), (0x100, '_global')):
                        if flags & bit:
                            regmap.setdefault(r, nm)
                            r += 1
                else:
                    n = struct.unpack_from('<H', body, q)[0]
                    q += 2
                    params = []
                    for _ in range(n):
                        s, q = cstr(body, q)
                        params.append(s)
                size = struct.unpack_from('<H', body, q)[0]
                fstart = nxt
                tag = '@%d' % at
                self.out.append(pad + 'function %s(%s) %s {' % (fname, ', '.join(params), tag))
                self.run(fstart, fstart + size, list(pool), indent + 1, regmap)
                self.out.append(pad + '}')
                if fname:
                    pass
                else:
                    st.append('function%s' % tag)
                p = fstart + size
                continue
            elif op == 0x94:
                emit('with (%s) { ... }' % pop())
            elif op == 0x8F:
                emit('try { ... }')
            elif op == 0x07:
                emit('stop();')
            elif op == 0x06:
                emit('play();')
            else:
                emit('// op %02x %s' % (op, body.hex()))
            p = nxt
        if st:
            emit('// leftover stack: ' + ', '.join(st))


def main():
    raw = open(sys.argv[1], 'rb').read()
    d = raw[:8] + zlib.decompress(raw[8:]) if raw[:3] == b'CWS' else raw
    nbits = d[8] >> 3
    pos = 8 + (5 + nbits * 4 + 7) // 8 + 4
    dec = Dec(d)
    while pos < len(d):
        h = struct.unpack_from('<H', d, pos)[0]
        pos += 2
        code, ln = h >> 6, h & 63
        if ln == 63:
            ln = struct.unpack_from('<I', d, pos)[0]
            pos += 4
        if code == 12:
            dec.out.append('=== DoAction @%d' % pos)
            dec.run(pos, pos + ln, [], 0, {})
        pos += ln
        if code == 0:
            break
    print('\n'.join(dec.out))


main()
