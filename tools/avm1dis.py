"""Minimal AVM1 (Flash 8) disassembler, used to read the website's compiled inventory widget (inv.swf).

usage: python3 avm1dis.py file.swf > listing.txt
"""
import struct
import sys
import zlib

NAMES = {
    0x04: 'NextFrame', 0x05: 'PrevFrame', 0x06: 'Play', 0x07: 'Stop', 0x0A: 'Add', 0x0B: 'Subtract',
    0x0C: 'Multiply', 0x0D: 'Divide', 0x0E: 'Equals', 0x0F: 'Less', 0x10: 'And', 0x11: 'Or', 0x12: 'Not',
    0x13: 'StringEquals', 0x14: 'StringLength', 0x15: 'StringExtract', 0x17: 'Pop', 0x18: 'ToInteger',
    0x1C: 'GetVariable', 0x1D: 'SetVariable', 0x20: 'SetTarget2', 0x21: 'StringAdd', 0x22: 'GetProperty',
    0x23: 'SetProperty', 0x24: 'CloneSprite', 0x25: 'RemoveSprite', 0x26: 'Trace', 0x27: 'StartDrag',
    0x28: 'EndDrag', 0x29: 'StringLess', 0x2A: 'Throw', 0x2B: 'CastOp', 0x2C: 'ImplementsOp', 0x30: 'RandomNumber',
    0x31: 'MBStringLength', 0x32: 'CharToAscii', 0x33: 'AsciiToChar', 0x34: 'GetTime', 0x35: 'MBStringExtract',
    0x36: 'MBCharToAscii', 0x37: 'MBAsciiToChar', 0x3A: 'Delete', 0x3B: 'Delete2', 0x3C: 'DefineLocal',
    0x3D: 'CallFunction', 0x3E: 'Return', 0x3F: 'Modulo', 0x40: 'NewObject', 0x41: 'DefineLocal2',
    0x42: 'InitArray', 0x43: 'InitObject', 0x44: 'TypeOf', 0x45: 'TargetPath', 0x46: 'Enumerate',
    0x47: 'Add2', 0x48: 'Less2', 0x49: 'Equals2', 0x4A: 'ToNumber', 0x4B: 'ToString', 0x4C: 'PushDuplicate',
    0x4D: 'StackSwap', 0x4E: 'GetMember', 0x4F: 'SetMember', 0x50: 'Increment', 0x51: 'Decrement',
    0x52: 'CallMethod', 0x53: 'NewMethod', 0x54: 'InstanceOf', 0x55: 'Enumerate2', 0x60: 'BitAnd', 0x61: 'BitOr',
    0x62: 'BitXor', 0x63: 'BitLShift', 0x64: 'BitRShift', 0x65: 'BitURShift', 0x66: 'StrictEquals',
    0x67: 'Greater', 0x68: 'StringGreater', 0x69: 'Extends', 0x81: 'GotoFrame', 0x83: 'GetURL',
    0x87: 'StoreRegister', 0x88: 'ConstantPool', 0x8A: 'WaitForFrame', 0x8B: 'SetTarget', 0x8C: 'GoToLabel',
    0x8D: 'WaitForFrame2', 0x8E: 'DefineFunction2', 0x8F: 'Try', 0x94: 'With', 0x96: 'Push', 0x99: 'Jump',
    0x9A: 'GetURL2', 0x9B: 'DefineFunction', 0x9D: 'If', 0x9E: 'Call', 0x9F: 'GotoFrame2',
}


def cstr(d, p):
    e = d.index(b'\0', p)
    return d[p:e].decode('latin-1'), e + 1


def dis(d, start, end, pool, out, indent=0):
    p = start
    pad = '  ' * indent
    while p < end:
        op = d[p]
        at = p
        p += 1
        ln = 0
        if op >= 0x80:
            ln = struct.unpack_from('<H', d, p)[0]
            p += 2
        body = d[p:p + ln]
        name = NAMES.get(op, 'op%02X' % op)
        arg = ''
        if op == 0x88:
            n = struct.unpack_from('<H', body, 0)[0]
            q = 2
            pool[:] = []
            for _ in range(n):
                s, q = cstr(body, q)
                pool.append(s)
            arg = '(%d strings)' % n
        elif op == 0x96:
            q = 0
            vals = []
            while q < ln:
                t = body[q]
                q += 1
                if t == 0:
                    s, q2 = cstr(body, q)
                    vals.append(repr(s))
                    q = q2
                elif t == 1:
                    vals.append(repr(struct.unpack_from('<f', body, q)[0]))
                    q += 4
                elif t == 2:
                    vals.append('null')
                elif t == 3:
                    vals.append('undefined')
                elif t == 4:
                    vals.append('r%d' % body[q])
                    q += 1
                elif t == 5:
                    vals.append('true' if body[q] else 'false')
                    q += 1
                elif t == 6:
                    hi, lo = struct.unpack_from('<II', body, q)
                    vals.append(repr(struct.unpack('<d', struct.pack('<II', lo, hi))[0]))
                    q += 8
                elif t == 7:
                    vals.append(str(struct.unpack_from('<i', body, q)[0]))
                    q += 4
                elif t == 8:
                    i = body[q]
                    vals.append(repr(pool[i]) if i < len(pool) else 'pool%d' % i)
                    q += 1
                elif t == 9:
                    i = struct.unpack_from('<H', body, q)[0]
                    vals.append(repr(pool[i]) if i < len(pool) else 'pool%d' % i)
                    q += 2
                else:
                    vals.append('?%d' % t)
                    break
            arg = ', '.join(vals)
        elif op in (0x99, 0x9D):
            off = struct.unpack_from('<h', body, 0)[0]
            arg = '-> %d' % (p + ln + off)
        elif op == 0x87:
            arg = 'r%d' % body[0]
        elif op == 0x8E:
            q = 0
            fname, q = cstr(body, q)
            nparams = struct.unpack_from('<H', body, q)[0]
            q += 2
            regs = body[q]
            q += 1
            flags = struct.unpack_from('<H', body, q)[0]
            q += 2
            params = []
            for _ in range(nparams):
                reg = body[q]
                q += 1
                pn, q = cstr(body, q)
                params.append('%s=r%d' % (pn, reg) if reg else pn)
            size = struct.unpack_from('<H', body, q)[0]
            # preloaded registers: this/arguments/super/_root/_parent/_global in order
            pre = []
            r = 1
            for bit, nm in ((0x01, 'this'), (0x04, 'arguments'), (0x10, 'super'), (0x40, '_root'),
                            (0x80, '_parent'), (0x100, '_global')):
                if flags & bit:
                    pre.append('%s=r%d' % (nm, r))
                    r += 1
            out.append('%s%5d function %s(%s) [%s] {' % (pad, at, fname, ', '.join(params), ', '.join(pre)))
            fstart = p + ln
            dis(d, fstart, fstart + size, list(pool), out, indent + 1)
            out.append('%s}' % pad)
            p = fstart + size
            continue
        elif op == 0x9B:
            q = 0
            fname, q = cstr(body, q)
            n = struct.unpack_from('<H', body, q)[0]
            q += 2
            params = []
            for _ in range(n):
                s, q = cstr(body, q)
                params.append(s)
            size = struct.unpack_from('<H', body, q)[0]
            out.append('%s%5d function %s(%s) {' % (pad, at, fname, ', '.join(params)))
            fstart = p + ln
            dis(d, fstart, fstart + size, list(pool), out, indent + 1)
            out.append('%s}' % pad)
            p = fstart + size
            continue
        elif op == 0x8F:
            arg = body.hex()
        elif ln:
            arg = body.hex()
        out.append('%s%5d %s %s' % (pad, at, name, arg))
        p += ln


def main():
    raw = open(sys.argv[1], 'rb').read()
    d = raw[:8] + zlib.decompress(raw[8:]) if raw[:3] == b'CWS' else raw
    nbits = d[8] >> 3
    pos = 8 + (5 + nbits * 4 + 7) // 8 + 4
    out = []
    while pos < len(d):
        h = struct.unpack_from('<H', d, pos)[0]
        pos += 2
        code, ln = h >> 6, h & 63
        if ln == 63:
            ln = struct.unpack_from('<I', d, pos)[0]
            pos += 4
        if code == 12:
            out.append('=== DoAction @%d' % pos)
            dis(d, pos, pos + ln, [], out)
        elif code == 59:
            sid = struct.unpack_from('<H', d, pos)[0]
            out.append('=== DoInitAction sprite %d' % sid)
            dis(d, pos + 2, pos + ln, [], out)
        pos += ln
        if code == 0:
            break
    print('\n'.join(out))


main()
