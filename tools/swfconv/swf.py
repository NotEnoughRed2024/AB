"""Minimal SWF (Flash 8 era) parser for the Alphabounce art library.

Parses the subset of tags the game's gfx.swf uses: shapes 1-4, sprites,
PlaceObject2/3 (with filters and blend modes), RemoveObject2, bitmaps,
DefineFont3, DefineText, DefineEditText, frame labels and simple frame
actions (stop/play/goto).
"""
import struct
import zlib


class Bits:
    def __init__(self, data, pos=0):
        self.d = data
        self.pos = pos  # byte position
        self.bit = 0    # bits consumed in current byte

    def align(self):
        if self.bit:
            self.bit = 0
            self.pos += 1

    def ub(self, n):
        v = 0
        for _ in range(n):
            byte = self.d[self.pos]
            v = (v << 1) | ((byte >> (7 - self.bit)) & 1)
            self.bit += 1
            if self.bit == 8:
                self.bit = 0
                self.pos += 1
        return v

    def sb(self, n):
        if n == 0:
            return 0
        v = self.ub(n)
        if v & (1 << (n - 1)):
            v -= 1 << n
        return v

    def fb(self, n):
        return self.sb(n) / 65536.0

    # byte-aligned readers
    def u8(self):
        self.align()
        v = self.d[self.pos]
        self.pos += 1
        return v

    def u16(self):
        self.align()
        v = struct.unpack_from('<H', self.d, self.pos)[0]
        self.pos += 2
        return v

    def s16(self):
        self.align()
        v = struct.unpack_from('<h', self.d, self.pos)[0]
        self.pos += 2
        return v

    def u32(self):
        self.align()
        v = struct.unpack_from('<I', self.d, self.pos)[0]
        self.pos += 4
        return v

    def fixed(self):  # 16.16
        self.align()
        v = struct.unpack_from('<i', self.d, self.pos)[0]
        self.pos += 4
        return v / 65536.0

    def fixed8(self):  # 8.8
        self.align()
        v = struct.unpack_from('<h', self.d, self.pos)[0]
        self.pos += 2
        return v / 256.0

    def f32(self):
        self.align()
        v = struct.unpack_from('<f', self.d, self.pos)[0]
        self.pos += 4
        return v

    def string(self):
        self.align()
        e = self.d.index(b'\0', self.pos)
        s = self.d[self.pos:e].decode('utf-8', 'replace')
        self.pos = e + 1
        return s

    def bytes(self, n):
        self.align()
        v = self.d[self.pos:self.pos + n]
        self.pos += n
        return v

    def rect(self):
        self.align()
        n = self.ub(5)
        r = [self.sb(n) for _ in range(4)]  # xmin xmax ymin ymax
        self.align()
        return r

    def rgb(self):
        return (self.u8(), self.u8(), self.u8(), 255)

    def rgba(self):
        return (self.u8(), self.u8(), self.u8(), self.u8())

    def argb(self):
        a = self.u8()
        return (self.u8(), self.u8(), self.u8(), a)

    def matrix(self):
        """Returns [a, b, c, d, tx, ty] with x' = a x + c y + tx, y' = b x + d y + ty (twips)."""
        self.align()
        a = d = 1.0
        b = c = 0.0
        if self.ub(1):
            n = self.ub(5)
            a = self.fb(n)
            d = self.fb(n)
        if self.ub(1):
            n = self.ub(5)
            b = self.fb(n)
            c = self.fb(n)
        n = self.ub(5)
        tx = self.sb(n)
        ty = self.sb(n)
        self.align()
        return [a, b, c, d, tx, ty]

    def cxform(self, alpha):
        """Returns [rm, gm, bm, am, ra, ga, ba, aa] (multipliers as floats, adds as ints)."""
        self.align()
        has_add = self.ub(1)
        has_mult = self.ub(1)
        n = self.ub(4)
        k = 4 if alpha else 3
        m = [1.0, 1.0, 1.0, 1.0]
        a = [0, 0, 0, 0]
        if has_mult:
            for i in range(k):
                m[i] = self.sb(n) / 256.0
        if has_add:
            for i in range(k):
                a[i] = self.sb(n)
        self.align()
        return m + a


def iter_tags(d, pos, end):
    while pos < end:
        h = struct.unpack_from('<H', d, pos)[0]
        pos += 2
        code = h >> 6
        ln = h & 63
        if ln == 63:
            ln = struct.unpack_from('<I', d, pos)[0]
            pos += 4
        yield code, pos, ln
        pos += ln
        if code == 0:
            break


# ---------------------------------------------------------------- shapes

def read_gradient(b, ver, focal):
    b.align()
    spread = b.ub(2)
    interp = b.ub(2)
    n = b.ub(4)
    stops = []
    for _ in range(n):
        r = b.u8()
        col = b.rgba() if ver >= 3 else b.rgb()
        stops.append((r, col))
    g = {'spread': spread, 'interp': interp, 'stops': stops}
    if focal:
        g['focal'] = b.fixed8()
    return g


def read_fill(b, ver):
    t = b.u8()
    if t == 0x00:
        return {'type': 'solid', 'color': b.rgba() if ver >= 3 else b.rgb()}
    if t in (0x10, 0x12, 0x13):
        m = b.matrix()
        g = read_gradient(b, ver, t == 0x13)
        return {'type': {0x10: 'linear', 0x12: 'radial', 0x13: 'focal'}[t], 'matrix': m, 'gradient': g}
    if 0x40 <= t <= 0x43:
        bid = b.u16()
        m = b.matrix()
        return {'type': 'bitmap', 'bitmap': bid, 'matrix': m,
                'repeat': t in (0x40, 0x42), 'smooth': t in (0x40, 0x41)}
    raise ValueError('unknown fill type %x' % t)


def read_fills(b, ver):
    n = b.u8()
    if n == 0xFF and ver >= 2:
        n = b.u16()
    return [read_fill(b, ver) for _ in range(n)]


def read_lines(b, ver):
    n = b.u8()
    if n == 0xFF and ver >= 2:
        n = b.u16()
    out = []
    for _ in range(n):
        w = b.u16()
        if ver >= 4:
            b.align()
            start_cap = b.ub(2)
            join = b.ub(2)
            has_fill = b.ub(1)
            b.ub(1); b.ub(1); b.ub(1)  # noHScale noVScale pixelHinting
            b.ub(5)
            b.ub(1)  # noClose
            end_cap = b.ub(2)
            miter = b.fixed8() if join == 2 else None
            if has_fill:
                fill = read_fill(b, ver)
                col = fill.get('color', (0, 0, 0, 255))
                if fill['type'] != 'solid':
                    # gradient stroke: approximate with first stop colour
                    col = fill['gradient']['stops'][0][1] if 'gradient' in fill else (0, 0, 0, 255)
            else:
                col = b.rgba()
            out.append({'width': w, 'color': col, 'cap': start_cap, 'join': join, 'miter': miter})
        else:
            col = b.rgba() if ver >= 3 else b.rgb()
            out.append({'width': w, 'color': col, 'cap': 0, 'join': 0, 'miter': None})
    return out


def read_shape_records(b, ver, fills, lines, nfb, nlb):
    """Returns list of groups: {'fills','lines','edges':[(f0,f1,ln,(x0,y0),(cx,cy)|None,(x1,y1))]}"""
    groups = [{'fills': fills, 'lines': lines, 'edges': []}]
    x = y = 0
    f0 = f1 = ln = 0
    while True:
        if b.ub(1) == 0:
            flags = b.ub(5)
            if flags == 0:
                break
            new_styles = flags & 16
            if flags & 1:
                n = b.ub(5)
                x = b.sb(n)
                y = b.sb(n)
            if flags & 2:
                f0 = b.ub(nfb)
            if flags & 4:
                f1 = b.ub(nfb)
            if flags & 8:
                ln = b.ub(nlb)
            if new_styles:
                fl = read_fills(b, ver)
                li = read_lines(b, ver)
                b.align()
                nfb = b.ub(4)
                nlb = b.ub(4)
                groups.append({'fills': fl, 'lines': li, 'edges': []})
        else:
            straight = b.ub(1)
            n = b.ub(4) + 2
            if straight:
                if b.ub(1):
                    dx = b.sb(n)
                    dy = b.sb(n)
                elif b.ub(1):
                    dx = 0
                    dy = b.sb(n)
                else:
                    dx = b.sb(n)
                    dy = 0
                p0 = (x, y)
                x += dx
                y += dy
                groups[-1]['edges'].append((f0, f1, ln, p0, None, (x, y)))
            else:
                cdx = b.sb(n)
                cdy = b.sb(n)
                adx = b.sb(n)
                ady = b.sb(n)
                p0 = (x, y)
                c = (x + cdx, y + cdy)
                x = c[0] + adx
                y = c[1] + ady
                groups[-1]['edges'].append((f0, f1, ln, p0, c, (x, y)))
    b.align()
    return groups


def parse_shape(data, ver):
    b = Bits(data)
    sid = b.u16()
    bounds = b.rect()
    if ver == 4:
        b.rect()  # edge bounds
        b.u8()
    fills = read_fills(b, ver)
    lines = read_lines(b, ver)
    b.align()
    nfb = b.ub(4)
    nlb = b.ub(4)
    groups = read_shape_records(b, ver, fills, lines, nfb, nlb)
    return sid, {'kind': 'shape', 'bounds': bounds, 'groups': groups}


def parse_glyph(data, pos):
    b = Bits(data, pos)
    nfb = b.ub(4)
    nlb = b.ub(4)
    groups = read_shape_records(b, 1, [{'type': 'solid', 'color': (0, 0, 0, 255)}], [], nfb, nlb)
    return groups


# ---------------------------------------------------------------- fonts & text

def parse_font3(data, ver):
    b = Bits(data)
    fid = b.u16()
    flags = b.u8()
    has_layout = flags & 0x80
    wide_offsets = flags & 0x08
    wide_codes = flags & 0x04
    italic = bool(flags & 0x02)
    bold = bool(flags & 0x01)
    b.u8()  # language
    nlen = b.u8()
    name = b.bytes(nlen).decode('latin-1').rstrip('\0')
    n = b.u16()
    base = b.pos
    offs = [(b.u32() if wide_offsets else b.u16()) for _ in range(n)]
    code_off = b.u32() if wide_offsets else b.u16()
    glyphs = []
    for i in range(n):
        glyphs.append(parse_glyph(data, base + offs[i]))
    b.pos = base + code_off
    b.bit = 0
    codes = [(b.u16() if wide_codes else b.u8()) for _ in range(n)]
    font = {'kind': 'font', 'name': name, 'bold': bold, 'italic': italic,
            'glyphs': glyphs, 'codes': codes, 'em': 20480 if ver == 3 else 1024}
    if has_layout:
        font['ascent'] = b.u16()
        font['descent'] = b.u16()
        font['leading'] = b.s16()
        font['advances'] = [b.s16() for _ in range(n)]
        font['glyph_bounds'] = [b.rect() for _ in range(n)]
    return fid, font


def parse_text(data, ver):
    b = Bits(data)
    tid = b.u16()
    bounds = b.rect()
    m = b.matrix()
    gb = b.u8()
    ab = b.u8()
    records = []
    font = None
    color = (0, 0, 0, 255)
    x = y = 0
    height = 0
    while True:
        b.align()
        flags = b.u8()
        if flags == 0:
            break
        if flags & 0x08:
            font = b.u16()
        if flags & 0x04:
            color = b.rgba() if ver == 2 else b.rgb()
        if flags & 0x01:
            x = b.s16()
        if flags & 0x02:
            y = b.s16()
        if flags & 0x08:
            height = b.u16()
        n = b.u8()
        glyphs = []
        for _ in range(n):
            gi = b.ub(gb)
            adv = b.sb(ab)
            glyphs.append((gi, x))
            x += adv
        records.append({'font': font, 'color': color, 'y': y, 'height': height, 'glyphs': glyphs})
    return tid, {'kind': 'text', 'bounds': bounds, 'matrix': m, 'records': records}


def parse_edit_text(data):
    b = Bits(data)
    tid = b.u16()
    bounds = b.rect()
    b.align()
    has_text = b.ub(1)
    word_wrap = b.ub(1)
    multiline = b.ub(1)
    password = b.ub(1)
    readonly = b.ub(1)
    has_color = b.ub(1)
    has_maxlen = b.ub(1)
    has_font = b.ub(1)
    has_font_class = b.ub(1)
    auto_size = b.ub(1)
    has_layout = b.ub(1)
    no_select = b.ub(1)
    border = b.ub(1)
    was_static = b.ub(1)
    html = b.ub(1)
    use_outlines = b.ub(1)
    e = {'kind': 'edittext', 'bounds': bounds, 'wordWrap': bool(word_wrap), 'multiline': bool(multiline),
         'html': bool(html), 'outlines': bool(use_outlines), 'autoSize': bool(auto_size), 'border': bool(border)}
    if has_font:
        e['font'] = b.u16()
    if has_font_class:
        e['fontClass'] = b.string()
    if has_font or has_font_class:
        e['height'] = b.u16()
    e['color'] = b.rgba() if has_color else (0, 0, 0, 255)
    if has_maxlen:
        e['maxLength'] = b.u16()
    if has_layout:
        e['align'] = b.u8()
        e['leftMargin'] = b.u16()
        e['rightMargin'] = b.u16()
        e['indent'] = b.u16()
        e['leading'] = b.s16()
    e['variable'] = b.string()
    e['text'] = b.string() if has_text else ''
    return tid, e


# ---------------------------------------------------------------- bitmaps

def parse_lossless(data, ver):
    from PIL import Image
    b = Bits(data)
    bid = b.u16()
    fmt = b.u8()
    w = b.u16()
    h = b.u16()
    if fmt == 3:
        ncol = b.u8() + 1
        raw = zlib.decompress(data[b.pos:])
        csz = 4 if ver == 2 else 3
        pal = [raw[i * csz:(i + 1) * csz] for i in range(ncol)]
        stride = (w + 3) & ~3
        px = raw[ncol * csz:]
        img = Image.new('RGBA', (w, h))
        out = []
        for yy in range(h):
            for xx in range(w):
                c = pal[px[yy * stride + xx]]
                out.append((c[0], c[1], c[2], c[3] if csz == 4 else 255))
        img.putdata(out)
    elif fmt == 5:
        raw = zlib.decompress(data[b.pos:])
        img = Image.new('RGBA', (w, h))
        out = []
        for i in range(w * h):
            a, r, g, bb = raw[i * 4:i * 4 + 4]
            if ver == 1:
                a = 255
            elif a:  # un-premultiply
                r = min(255, r * 255 // a)
                g = min(255, g * 255 // a)
                bb = min(255, bb * 255 // a)
            out.append((r, g, bb, a))
        img.putdata(out)
    else:
        raise ValueError('lossless fmt %d' % fmt)
    return bid, {'kind': 'bitmap', 'image': img}


def parse_jpeg3(data, jpeg_tables=None):
    from PIL import Image
    import io
    b = Bits(data)
    bid = b.u16()
    alpha_off = b.u32()
    jpg = data[6:6 + alpha_off]
    # strip erroneous SOI/EOI pairs some Flash exporters emit
    if jpg[:4] == b'\xff\xd9\xff\xd8':
        jpg = jpg[4:]
    img = Image.open(io.BytesIO(jpg)).convert('RGBA')
    alpha = zlib.decompress(data[6 + alpha_off:])
    if len(alpha) >= img.width * img.height:
        a = Image.frombytes('L', img.size, alpha[:img.width * img.height])
        img.putalpha(a)
    return bid, {'kind': 'bitmap', 'image': img}


# ---------------------------------------------------------------- display list

def read_filters(b):
    n = b.u8()
    out = []
    for _ in range(n):
        fid = b.u8()
        if fid == 0:  # drop shadow
            col = b.rgba()
            bx = b.fixed(); by = b.fixed(); ang = b.fixed(); dist = b.fixed(); st = b.fixed8()
            fl = b.u8()
            out.append({'type': 'dropShadow', 'color': col, 'blurX': bx, 'blurY': by, 'angle': ang,
                        'distance': dist, 'strength': st, 'inner': bool(fl & 0x80),
                        'knockout': bool(fl & 0x40), 'passes': fl & 0x1F})
        elif fid == 1:
            bx = b.fixed(); by = b.fixed(); p = b.u8()
            out.append({'type': 'blur', 'blurX': bx, 'blurY': by, 'passes': p >> 3})
        elif fid == 2:
            col = b.rgba()
            bx = b.fixed(); by = b.fixed(); st = b.fixed8(); fl = b.u8()
            out.append({'type': 'glow', 'color': col, 'blurX': bx, 'blurY': by, 'strength': st,
                        'inner': bool(fl & 0x80), 'knockout': bool(fl & 0x40), 'passes': fl & 0x1F})
        elif fid == 3:
            sc = b.rgba(); hc = b.rgba()
            bx = b.fixed(); by = b.fixed(); ang = b.fixed(); dist = b.fixed(); st = b.fixed8(); fl = b.u8()
            out.append({'type': 'bevel', 'shadow': sc, 'highlight': hc, 'blurX': bx, 'blurY': by,
                        'angle': ang, 'distance': dist, 'strength': st, 'passes': fl & 0x0F})
        elif fid in (4, 7):
            nc = b.u8()
            cols = [b.rgba() for _ in range(nc)]
            rats = [b.u8() for _ in range(nc)]
            bx = b.fixed(); by = b.fixed(); ang = b.fixed(); dist = b.fixed(); st = b.fixed8(); fl = b.u8()
            out.append({'type': 'gradientGlow' if fid == 4 else 'gradientBevel', 'colors': cols,
                        'ratios': rats, 'blurX': bx, 'blurY': by, 'strength': st})
        elif fid == 5:
            mx = b.u8(); my = b.u8()
            b.f32(); b.f32()
            for _ in range(mx * my):
                b.f32()
            b.rgba(); b.u8()
            out.append({'type': 'convolution'})
        elif fid == 6:
            out.append({'type': 'colorMatrix', 'matrix': [b.f32() for _ in range(20)]})
        else:
            raise ValueError('filter %d' % fid)
    return out


BLEND = {0: 'normal', 1: 'normal', 2: 'layer', 3: 'multiply', 4: 'screen', 5: 'lighten', 6: 'darken',
         7: 'difference', 8: 'add', 9: 'subtract', 10: 'invert', 11: 'alpha', 12: 'erase', 13: 'overlay',
         14: 'hardlight'}


def parse_place(data, ver):
    b = Bits(data)
    f = b.u8()
    f2 = b.u8() if ver == 3 else 0
    p = {'depth': b.u16(), 'move': bool(f & 1)}
    if ver == 3 and (f2 & 0x08 or (f2 & 0x10 and f & 0x02)):
        p['className'] = b.string()
    if f & 0x02:
        p['char'] = b.u16()
    if f & 0x04:
        p['matrix'] = b.matrix()
    if f & 0x08:
        p['cxform'] = b.cxform(True)
    if f & 0x10:
        p['ratio'] = b.u16()
    if f & 0x20:
        p['name'] = b.string()
    if f & 0x40:
        p['clipDepth'] = b.u16()
    if ver == 3:
        if f2 & 0x01:
            p['filters'] = read_filters(b)
        if f2 & 0x02:
            p['blend'] = BLEND.get(b.u8(), 'normal')
        if f2 & 0x04:
            p['cacheAsBitmap'] = bool(b.u8())
        if f2 & 0x20:
            p['visible'] = bool(b.u8())
    return p


def parse_actions(data):
    """Very small AVM1 reader: extracts stop/play/gotoFrame/gotoLabel per frame."""
    out = []
    pos = 0
    while pos < len(data):
        code = data[pos]
        pos += 1
        if code == 0:
            break
        ln = 0
        payload = b''
        if code >= 0x80:
            ln = struct.unpack_from('<H', data, pos)[0]
            pos += 2
            payload = data[pos:pos + ln]
            pos += ln
        if code == 0x07:
            out.append(('stop',))
        elif code == 0x06:
            out.append(('play',))
        elif code == 0x04:
            out.append(('nextFrame',))
        elif code == 0x05:
            out.append(('prevFrame',))
        elif code == 0x81:
            out.append(('gotoFrame', struct.unpack('<H', payload[:2])[0]))
        elif code == 0x8C:
            out.append(('gotoLabel', payload.rstrip(b'\0').decode('latin-1')))
        else:
            out.append(('other', code))
    return out


def parse_timeline(d, pos, end, swf):
    """Returns a list of frames: each frame = {'ops': [...], 'actions': [...], 'label': str|None}."""
    frames = []
    cur = {'ops': [], 'actions': [], 'label': None}
    for code, p, ln in iter_tags(d, pos, end):
        data = d[p:p + ln]
        if code == 1:
            frames.append(cur)
            cur = {'ops': [], 'actions': [], 'label': None}
        elif code == 26:
            cur['ops'].append(('place', parse_place(data, 2)))
        elif code == 70:
            cur['ops'].append(('place', parse_place(data, 3)))
        elif code == 4:
            b = Bits(data)
            cid = b.u16()
            dep = b.u16()
            pl = {'depth': dep, 'char': cid, 'move': False}
            if b.pos < len(data):
                pl['matrix'] = b.matrix()
            cur['ops'].append(('place', pl))
        elif code == 28:
            cur['ops'].append(('remove', struct.unpack('<H', data[:2])[0]))
        elif code == 5:
            cur['ops'].append(('remove', struct.unpack('<H', data[2:4])[0]))
        elif code == 43:
            cur['label'] = data.split(b'\0')[0].decode('latin-1')
        elif code == 12:
            cur['actions'] += parse_actions(data)
        elif code == 0:
            break
        else:
            swf.handle_definition(code, data)
    if cur['ops'] or cur['actions']:
        frames.append(cur)
    return frames


class SWF:
    def __init__(self, path):
        raw = open(path, 'rb').read()
        if raw[:3] == b'CWS':
            self.d = raw[:8] + zlib.decompress(raw[8:])
        elif raw[:3] == b'FWS':
            self.d = raw
        else:
            raise ValueError('unsupported swf signature')
        self.version = raw[3]
        b = Bits(self.d, 8)
        r = b.rect()
        self.width = (r[1] - r[0]) / 20
        self.height = (r[3] - r[2]) / 20
        b.align()
        self.fps = b.u16() / 256.0
        b.u16()
        self.chars = {}
        self.exports = {}
        self.unknown = {}
        self.root = parse_timeline(self.d, b.pos, len(self.d), self)

    def handle_definition(self, code, data):
        if code in (2, 22, 32, 83):
            ver = {2: 1, 22: 2, 32: 3, 83: 4}[code]
            cid, c = parse_shape(data, ver)
        elif code == 39:
            cid = struct.unpack('<H', data[:2])[0]
            n = struct.unpack('<H', data[2:4])[0]
            frames = parse_timeline(data, 4, len(data), self)
            c = {'kind': 'sprite', 'frames': frames, 'frameCount': n}
        elif code == 75:
            cid, c = parse_font3(data, 3)
        elif code == 48:
            cid, c = parse_font3(data, 2)
        elif code in (11, 33):
            cid, c = parse_text(data, 1 if code == 11 else 2)
        elif code == 37:
            cid, c = parse_edit_text(data)
        elif code in (20, 36):
            cid, c = parse_lossless(data, 1 if code == 20 else 2)
        elif code == 35:
            cid, c = parse_jpeg3(data)
        elif code == 56:
            b = Bits(data)
            for _ in range(b.u16()):
                cid = b.u16()
                self.exports[b.string()] = cid
            return
        elif code == 88:
            cid = struct.unpack('<H', data[:2])[0]
            if cid in self.chars:
                self.chars[cid]['fontName'] = data[2:].split(b'\0')[0].decode('utf-8', 'replace')
            return
        else:
            self.unknown[code] = self.unknown.get(code, 0) + 1
            return
        self.chars[cid] = c
