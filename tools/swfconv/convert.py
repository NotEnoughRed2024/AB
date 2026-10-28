"""Convert a Flash 8 art library (gfx.swf) into PNG atlases + JSON for the SDL3 port.

Every vector shape, static text and bitmap is rasterised once (via headless
Chromium rendering an SVG), packed into atlas pages, and the animated symbols
(sprites) are written out as per-frame display lists that the C++ engine
replays, so the game code can keep using gotoAndStop(), nested clips, etc.

usage: python3 convert.py gfx.swf out_dir [--scale 2]
"""
import argparse
import base64
import io
import json
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import swf as swflib  # noqa: E402

PAD = 2  # transparent pixels around each rasterised image


# ------------------------------------------------------------------ SVG

def css(col):
    return '#%02x%02x%02x' % col[:3]


def edge_path(edges):
    """Chain directed edges into closed sub-paths; returns SVG path data in twips."""
    by_start = {}
    for i, e in enumerate(edges):
        by_start.setdefault(e[0], []).append(i)
    used = [False] * len(edges)
    out = []
    for i in range(len(edges)):
        if used[i]:
            continue
        e = edges[i]
        used[i] = True
        start = e[0]
        out.append('M%d %d' % start)
        cur = e
        while True:
            p0, c, p1 = cur
            if c is None:
                out.append('L%d %d' % p1)
            else:
                out.append('Q%d %d %d %d' % (c[0], c[1], p1[0], p1[1]))
            if p1 == start:
                out.append('Z')
                break
            nxt = None
            for j in by_start.get(p1, ()):
                if not used[j]:
                    nxt = j
                    break
            if nxt is None:
                break
            used[nxt] = True
            cur = edges[nxt]
    return ' '.join(out)


class SvgBuilder:
    def __init__(self, lib):
        self.lib = lib
        self.defs = []
        self.n = 0

    def uid(self):
        self.n += 1
        return 'd%d' % self.n

    def paint(self, fill):
        """Returns (fill attr string) creating defs as required."""
        t = fill['type']
        if t == 'solid':
            c = fill['color']
            return 'fill="%s" fill-opacity="%.4f"' % (css(c), c[3] / 255)
        if t in ('linear', 'radial', 'focal'):
            g = fill['gradient']
            m = fill['matrix']
            gid = self.uid()
            spread = ['pad', 'reflect', 'repeat'][min(g['spread'], 2)]
            stops = ''.join('<stop offset="%.4f" stop-color="%s" stop-opacity="%.4f"/>' %
                            (r / 255, css(c), c[3] / 255) for r, c in g['stops'])
            tr = 'matrix(%.6f %.6f %.6f %.6f %.2f %.2f)' % tuple(m)
            ci = ' color-interpolation="linearRGB"' if g['interp'] == 1 else ''
            if t == 'linear':
                self.defs.append('<linearGradient id="%s" gradientUnits="userSpaceOnUse" x1="-16384" y1="0" '
                                 'x2="16384" y2="0" spreadMethod="%s" gradientTransform="%s"%s>%s</linearGradient>'
                                 % (gid, spread, tr, ci, stops))
            else:
                fx = g.get('focal', 0) * 16384
                self.defs.append('<radialGradient id="%s" gradientUnits="userSpaceOnUse" cx="0" cy="0" r="16384" '
                                 'fx="%.1f" fy="0" spreadMethod="%s" gradientTransform="%s"%s>%s</radialGradient>'
                                 % (gid, fx, spread, tr, ci, stops))
            return 'fill="url(#%s)"' % gid
        if t == 'bitmap':
            bm = self.lib.bitmap_data_uri(fill['bitmap'])
            if bm is None:
                return 'fill="none"'
            uri, w, h = bm
            pid = self.uid()
            m = fill['matrix']
            render = '' if fill['smooth'] else ' style="image-rendering:pixelated"'
            self.defs.append('<pattern id="%s" patternUnits="userSpaceOnUse" width="%d" height="%d" '
                             'patternTransform="matrix(%.6f %.6f %.6f %.6f %.2f %.2f)">'
                             '<image href="%s" width="%d" height="%d"%s/></pattern>'
                             % (pid, w, h, m[0], m[1], m[2], m[3], m[4], m[5], uri, w, h, render))
            return 'fill="url(#%s)"' % pid
        return 'fill="none"'

    def groups(self, groups, min_line_twips):
        body = []
        for g in groups:
            per_fill = {}
            per_line = {}
            for f0, f1, ln, p0, c, p1 in g['edges']:
                if f1:
                    per_fill.setdefault(f1, []).append((p0, c, p1))
                if f0:
                    per_fill.setdefault(f0, []).append((p1, c, p0))
                if ln:
                    per_line.setdefault(ln, []).append((p0, c, p1))
            for fi in sorted(per_fill):
                if fi - 1 >= len(g['fills']):
                    continue
                d = edge_path(per_fill[fi])
                body.append('<path fill-rule="evenodd" %s d="%s"/>' % (self.paint(g['fills'][fi - 1]), d))
            for li in sorted(per_line):
                if li - 1 >= len(g['lines']):
                    continue
                ls = g['lines'][li - 1]
                parts = []
                last = None
                for p0, c, p1 in per_line[li]:
                    if p0 != last:
                        parts.append('M%d %d' % p0)
                    parts.append(('L%d %d' % p1) if c is None else ('Q%d %d %d %d' % (c[0], c[1], p1[0], p1[1])))
                    last = p1
                col = ls['color']
                w = max(ls['width'], min_line_twips)
                cap = ['round', 'butt', 'square'][min(ls['cap'], 2)]
                join = ['round', 'bevel', 'miter'][min(ls['join'], 2)]
                body.append('<path fill="none" stroke="%s" stroke-opacity="%.4f" stroke-width="%d" '
                            'stroke-linecap="%s" stroke-linejoin="%s" d="%s"/>'
                            % (css(col), col[3] / 255, w, cap, join, ' '.join(parts)))
        return ''.join(body)


def make_svg(builder, content, bounds_tw, scale):
    """bounds_tw = [xmin,xmax,ymin,ymax] twips. Returns (svg, w, h, ox, oy) where (ox,oy) = image pixel of origin."""
    xmin, xmax, ymin, ymax = bounds_tw
    k = scale / 20.0
    w = int(math.ceil((xmax - xmin) * k)) + 2 * PAD
    h = int(math.ceil((ymax - ymin) * k)) + 2 * PAD
    ox = -xmin * k + PAD
    oy = -ymin * k + PAD
    svg = ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d"><defs>%s</defs>'
           '<g transform="translate(%.3f %.3f) scale(%.6f)">%s</g></svg>'
           % (w, h, ''.join(builder.defs), ox, oy, k, content))
    return svg, max(w, 1), max(h, 1), ox, oy


def transform_bounds(b, m):
    xmin, xmax, ymin, ymax = b
    pts = [(xmin, ymin), (xmax, ymin), (xmin, ymax), (xmax, ymax)]
    xs = [m[0] * x + m[2] * y + m[4] for x, y in pts]
    ys = [m[1] * x + m[3] * y + m[5] for x, y in pts]
    return [min(xs), max(xs), min(ys), max(ys)]


# ------------------------------------------------------------------ actions

def classify_actions(raw):
    """Map AVM1 snippets found in gfx.swf onto engine-level frame actions."""
    if not raw:
        return []
    kinds = [a[0] for a in raw]
    codes = [a[1] if a[0] == 'other' else None for a in raw]
    if kinds == ['stop']:
        return ['stop']
    if kinds == ['play']:
        return ['play']
    # random frame: ... getProp(_totalframes) random push 1 add gotoFrame2
    if 0x30 in codes and 0x9f in codes and 0x23 not in codes:
        return ['randomFrame']
    if 0x25 in codes:
        return ['removeSelf']
    if 0x52 in codes:
        return ['callObjKill']
    if 0x23 in codes and 0x30 in codes:
        return ['randomRotation']
    if 0x23 in codes and 0x0b in codes:
        return ['counterParentRotation']
    if 0x23 in codes and 0x47 in codes:
        return ['rotate2']
    if ('gotoFrame', 0) in raw and 'play' in kinds:
        return ['gotoAndPlay1']
    out = []
    for a in raw:
        if a[0] in ('stop', 'play'):
            out.append(a[0])
        elif a[0] == 'gotoFrame':
            out.append('goto:%d' % (a[1] + 1))
    return out


# ------------------------------------------------------------------ library

class Library:
    def __init__(self, swf, scale):
        self.swf = swf
        self.scale = scale
        self.images = []        # list of dicts: key, svg|pil, w, h, ox, oy
        self.image_index = {}   # key -> index
        self._bm_uri = {}

    def bitmap_data_uri(self, bid):
        if bid in self._bm_uri:
            return self._bm_uri[bid]
        c = self.swf.chars.get(bid)
        if not c or c['kind'] != 'bitmap':
            self._bm_uri[bid] = None
            return None
        buf = io.BytesIO()
        c['image'].save(buf, 'PNG')
        uri = 'data:image/png;base64,' + base64.b64encode(buf.getvalue()).decode()
        self._bm_uri[bid] = (uri, c['image'].width, c['image'].height)
        return self._bm_uri[bid]

    def add_image(self, key, svg, w, h, ox, oy):
        self.image_index[key] = len(self.images)
        self.images.append({'key': key, 'svg': svg, 'w': w, 'h': h, 'ox': ox, 'oy': oy})

    def glyph_content(self, font, gi, height_tw, x_tw, y_tw, color):
        g = font['glyphs'][gi]
        b = SvgBuilder(self)
        k = height_tw / font['em']
        path = []
        for grp in g:
            edges = []
            for f0, f1, ln, p0, c, p1 in grp['edges']:
                if f1:
                    edges.append((p0, c, p1))
                if f0:
                    edges.append((p1, c, p0))
            if edges:
                path.append(edge_path(edges))
        if not path:
            return ''
        return ('<path fill-rule="nonzero" fill="%s" fill-opacity="%.4f" '
                'transform="translate(%.2f %.2f) scale(%.6f)" d="%s"/>'
                % (css(color), color[3] / 255, x_tw, y_tw, k, ' '.join(path)))

    def build(self):
        chars = self.swf.chars
        out_chars = {}
        min_line = 20.0 / self.scale * 1.0  # hairlines: at least one output pixel
        for cid, c in sorted(chars.items()):
            k = c['kind']
            if k == 'shape':
                b = SvgBuilder(self)
                content = b.groups(c['groups'], min_line)
                svg, w, h, ox, oy = make_svg(b, content, c['bounds'], self.scale)
                self.add_image(cid, svg, w, h, ox, oy)
                out_chars[cid] = {'type': 'shape', 'image': self.image_index[cid]}
            elif k == 'text':
                parts = []
                for r in c['records']:
                    f = chars.get(r['font'])
                    if not f:
                        continue
                    for gi, x in r['glyphs']:
                        parts.append(self.glyph_content(f, gi, r['height'], x, r['y'], r['color']))
                m = c['matrix']
                content = '<g transform="matrix(%.6f %.6f %.6f %.6f %.2f %.2f)">%s</g>' % (
                    m[0], m[1], m[2], m[3], m[4], m[5], ''.join(parts))
                # DefineText bounds are already in the parent's space
                b = SvgBuilder(self)
                svg, w, h, ox, oy = make_svg(b, content, c['bounds'], self.scale)
                self.add_image(cid, svg, w, h, ox, oy)
                out_chars[cid] = {'type': 'shape', 'image': self.image_index[cid]}
            elif k == 'bitmap':
                img = c['image']
                buf = io.BytesIO()
                img.save(buf, 'PNG')
                uri = 'data:image/png;base64,' + base64.b64encode(buf.getvalue()).decode()
                bounds = [0, img.width * 20, 0, img.height * 20]
                content = '<image href="%s" width="%d" height="%d" transform="scale(20)"/>' % (
                    uri, img.width, img.height)
                svg, w, h, ox, oy = make_svg(SvgBuilder(self), content, bounds, self.scale)
                self.add_image(('bitmap', cid), svg, w, h, ox, oy)
                out_chars[cid] = {'type': 'bitmap', 'image': self.image_index[('bitmap', cid)],
                                  'width': img.width, 'height': img.height}
            elif k == 'edittext':
                e = dict(c)
                e.pop('kind')
                e['type'] = 'edittext'
                e['bounds'] = [v / 20.0 for v in e['bounds']]
                e['height'] = e.get('height', 240) / 20.0
                for key in ('leftMargin', 'rightMargin', 'indent', 'leading'):
                    if key in e:
                        e[key] = e[key] / 20.0
                out_chars[cid] = e
            elif k == 'sprite':
                out_chars[cid] = self.compile_sprite(c)
            elif k == 'font':
                out_chars[cid] = {'type': 'font', 'name': c.get('fontName') or c['name'],
                                  'bold': c['bold'], 'italic': c['italic']}
        self.chars = out_chars
        self.build_fonts()

    def compile_sprite(self, c):
        frames = []
        dl = {}       # depth -> record
        inst = [0]
        labels = {}
        for fi, f in enumerate(c['frames']):
            for op, arg in f['ops']:
                if op == 'remove':
                    dl.pop(arg, None)
                    continue
                p = arg
                d = p['depth']
                prev = dl.get(d)
                if prev is None or (not p['move']) or ('char' in p and p['char'] != prev['c']):
                    if 'char' not in p and prev is None:
                        continue
                    inst[0] += 1
                    rec = {'c': p.get('char', prev['c'] if prev else 0), 'i': inst[0], 'd': d,
                           'm': [1, 0, 0, 1, 0, 0]}
                    if prev is not None and p['move']:
                        # character replaced in place: keep transform
                        for key in ('m', 'cx', 'n', 'f', 'b', 'cd'):
                            if key in prev:
                                rec[key] = prev[key]
                else:
                    rec = dict(prev)
                if 'matrix' in p:
                    m = p['matrix']
                    rec['m'] = [round(m[0], 5), round(m[1], 5), round(m[2], 5), round(m[3], 5),
                                round(m[4] / 20.0, 3), round(m[5] / 20.0, 3)]
                if 'cxform' in p:
                    cx = p['cxform']
                    if cx == [1.0, 1.0, 1.0, 1.0, 0, 0, 0, 0]:
                        rec.pop('cx', None)
                    else:
                        rec['cx'] = cx
                if 'name' in p:
                    rec['n'] = p['name']
                if 'clipDepth' in p:
                    rec['cd'] = p['clipDepth']
                if 'filters' in p:
                    fl = [ff for ff in (simplify_filter(x) for x in p['filters']) if ff]
                    if fl:
                        rec['f'] = fl
                    else:
                        rec.pop('f', None)
                if 'blend' in p:
                    if p['blend'] in ('normal', 'layer'):
                        rec.pop('b', None)
                    else:
                        rec['b'] = p['blend']
                if 'ratio' in p:
                    rec['r'] = p['ratio']
                dl[d] = rec
            fr = {'dl': [dl[k] for k in sorted(dl)]}
            acts = classify_actions(f['actions'])
            if acts:
                fr['a'] = acts
            if f['label']:
                fr['label'] = f['label']
                labels[f['label']] = fi + 1
            frames.append(fr)
        # pad to declared frame count
        while len(frames) < c['frameCount']:
            frames.append({'dl': frames[-1]['dl'] if frames else []})
        return {'type': 'sprite', 'frames': frames, 'labels': labels}

    # ---------------------------------------------------------- fonts
    def build_fonts(self):
        """Rasterise every glyph of every embedded font (white, alpha-only) for dynamic text."""
        self.fonts = {}
        EM_PX = 32 * self.scale  # glyph raster size (pixels per em)
        for cid, c in self.swf.chars.items():
            if c['kind'] != 'font':
                continue
            glyphs = []
            for gi, code in enumerate(c['codes']):
                gb = c['glyph_bounds'][gi] if 'glyph_bounds' in c else None
                # compute bounds from outline
                pts = [p for grp in c['glyphs'][gi] for e in grp['edges'] for p in (e[3], e[5]) + ((e[4],) if e[4] else ())]
                adv = c['advances'][gi] if 'advances' in c else c['em']
                entry = {'code': code, 'advance': adv / c['em']}
                if pts:
                    xs = [p[0] for p in pts]
                    ys = [p[1] for p in pts]
                    # express bounds in "twips" at a nominal 20*EM_PX/scale text height
                    k = (EM_PX / self.scale * 20) / c['em']
                    bounds = [min(xs) * k, max(xs) * k, min(ys) * k, max(ys) * k]
                    content = self.glyph_content(c, gi, EM_PX / self.scale * 20, 0, 0, (255, 255, 255, 255))
                    svg, w, h, ox, oy = make_svg(SvgBuilder(self), content, bounds, self.scale)
                    key = ('glyph', cid, gi)
                    self.add_image(key, svg, w, h, ox, oy)
                    entry['image'] = self.image_index[key]
                glyphs.append(entry)
            f = {'name': c.get('fontName') or c['name'], 'bold': c['bold'], 'italic': c['italic'],
                 'emPx': EM_PX / self.scale, 'glyphs': glyphs}
            if 'ascent' in c:
                f['ascent'] = c['ascent'] / c['em']
                f['descent'] = c['descent'] / c['em']
                f['leading'] = c['leading'] / c['em']
            self.fonts[cid] = f


def simplify_filter(f):
    t = f['type']
    if t == 'glow':
        return {'t': 'glow', 'c': list(f['color']), 'bx': round(f['blurX'], 2), 'by': round(f['blurY'], 2),
                's': round(f['strength'], 3), 'inner': f['inner'], 'knockout': f['knockout']}
    if t == 'blur':
        return {'t': 'blur', 'bx': round(f['blurX'], 2), 'by': round(f['blurY'], 2)}
    if t == 'dropShadow':
        return {'t': 'shadow', 'c': list(f['color']), 'bx': round(f['blurX'], 2), 'by': round(f['blurY'], 2),
                's': round(f['strength'], 3), 'angle': round(f['angle'], 4), 'dist': round(f['distance'], 2),
                'inner': f['inner'], 'knockout': f['knockout']}
    if t == 'colorMatrix':
        return {'t': 'colorMatrix', 'm': [round(v, 5) for v in f['matrix']]}
    return None


# ------------------------------------------------------------------ rasterise + pack

RENDER_JS = """
async (items) => {
  const out = [];
  for (const it of items) {
    const img = new Image();
    const blob = new Blob([it.svg], {type: 'image/svg+xml'});
    const url = URL.createObjectURL(blob);
    await new Promise((res, rej) => { img.onload = res; img.onerror = () => res(); img.src = url; });
    const c = document.createElement('canvas');
    c.width = it.w; c.height = it.h;
    const ctx = c.getContext('2d');
    try { ctx.drawImage(img, 0, 0); } catch (e) {}
    URL.revokeObjectURL(url);
    out.push(c.toDataURL('image/png').split(',')[1]);
  }
  return out;
}
"""


def rasterise(images):
    from PIL import Image
    from playwright.sync_api import sync_playwright
    with sync_playwright() as p:
        br = p.chromium.launch()
        pg = br.new_page()
        pg.set_content('<html><body></body></html>')
        B = 40
        for i in range(0, len(images), B):
            batch = images[i:i + B]
            res = pg.evaluate(RENDER_JS, [{'svg': im['svg'], 'w': im['w'], 'h': im['h']} for im in batch])
            for im, b64 in zip(batch, res):
                im['pil'] = Image.open(io.BytesIO(base64.b64decode(b64))).convert('RGBA')
            sys.stderr.write('\r  rasterised %d/%d' % (min(i + B, len(images)), len(images)))
        sys.stderr.write('\n')
        br.close()


def pack(images, page_size):
    """Simple shelf packer. Sets im['page'], im['x'], im['y']. Returns page count."""
    order = sorted(range(len(images)), key=lambda i: -images[i]['h'])
    pages = 0
    x = y = shelf_h = 0
    started = False
    for i in order:
        im = images[i]
        w, h = im['w'], im['h']
        if w > page_size or h > page_size:
            raise ValueError('image %r too large (%dx%d)' % (im['key'], w, h))
        if not started:
            pages, x, y, shelf_h, started = 1, 0, 0, 0, True
        if x + w > page_size:
            x = 0
            y += shelf_h
            shelf_h = 0
        if y + h > page_size:
            pages += 1
            x = y = shelf_h = 0
        im['page'] = pages - 1
        im['x'] = x
        im['y'] = y
        x += w
        shelf_h = max(shelf_h, h)
    return pages


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('swf')
    ap.add_argument('out')
    ap.add_argument('--scale', type=float, default=2.0)
    ap.add_argument('--page', type=int, default=4096)
    ap.add_argument('--name', default='gfx')
    a = ap.parse_args()

    from PIL import Image
    s = swflib.SWF(a.swf)
    lib = Library(s, a.scale)
    lib.build()
    print('%s: %d images to rasterise' % (a.name, len(lib.images)))
    rasterise(lib.images)
    # trim fully transparent images to 1x1 so they don't waste atlas space
    for im in lib.images:
        bb = im['pil'].getbbox()
        if bb is None:
            im['pil'] = Image.new('RGBA', (1, 1))
            im['w'] = im['h'] = 1
    npages = pack(lib.images, a.page)
    os.makedirs(a.out, exist_ok=True)
    pages = [Image.new('RGBA', (a.page, a.page)) for _ in range(npages)]
    for im in lib.images:
        pages[im['page']].paste(im['pil'], (im['x'], im['y']))
    for i, pg in enumerate(pages):
        bb = pg.getbbox() or (0, 0, 1, 1)
        h = 1 << max(0, (bb[3] - 1).bit_length())
        pg.crop((0, 0, a.page, min(a.page, max(h, 1)))).save(os.path.join(a.out, '%s_%d.png' % (a.name, i)), optimize=True)

    lib_json = {
        'name': a.name,
        'scale': a.scale,
        'stage': [s.width, s.height],
        'fps': s.fps,
        'pages': ['%s_%d.png' % (a.name, i) for i in range(npages)],
        'images': [[im['page'], im['x'], im['y'], im['w'], im['h'], round(im['ox'], 3), round(im['oy'], 3)]
                   for im in lib.images],
        'chars': {str(k): v for k, v in lib.chars.items()},
        'fonts': {str(k): v for k, v in lib.fonts.items()},
        'exports': s.exports,
        'root': lib.compile_sprite({'frames': s.root, 'frameCount': len(s.root)})['frames'],
    }
    with open(os.path.join(a.out, '%s.json' % a.name), 'w') as f:
        json.dump(lib_json, f, separators=(',', ':'))
    print('wrote %d atlas page(s), %d chars, %d exports' % (npages, len(lib.chars), len(s.exports)))


if __name__ == '__main__':
    main()
