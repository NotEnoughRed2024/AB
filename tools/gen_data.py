"""Extract Alphabounce's game data from the original Haxe sources into JSON.

Reads (from Motion Twin's WebGamesArchives/Alphabounce):
  com/MissionInfo.hx  -> missions + item positions (the real "web" list, not the decoy)
  com/ZoneInfo.hx     -> planets (real "web" list), black holes
  com/ShopInfo.hx     -> shop items
  client/src/Text{En,Fr,De,Es}.hx -> UI text per language
  web/xml/{en,fr,de,es}/texts.xml -> website texts (mission feed, vignettes) -> site_<lang>.json

usage: python3 gen_data.py <path to Alphabounce folder> <out dir>
"""
import json
import math
import os
import re
import sys


def read(path):
    raw = open(path, 'rb').read()
    for enc in ('utf-8-sig', 'cp1252', 'latin-1'):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            pass


def strip_comments(src):
    out = []
    i = 0
    n = len(src)
    while i < n:
        c = src[i]
        if c == '"' or c == "'":
            q = c
            j = i + 1
            while j < n and src[j] != q:
                if src[j] == '\\':
                    j += 1
                j += 1
            out.append(src[i:j + 1])
            i = j + 1
        elif src.startswith('//', i):
            j = src.find('\n', i)
            i = n if j < 0 else j
        elif src.startswith('/*', i):
            j = src.find('*/', i + 2)
            i = n if j < 0 else j + 2
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def constants(src, prefix=''):
    """static var NAME = <int expression>;  (evaluated in order)"""
    env = {}
    for m in re.finditer(r'static\s+var\s+([A-Z_][A-Z0-9_]*)\s*(?::\s*\w+)?\s*=\s*([^;]+);', src):
        name, expr = m.group(1), m.group(2).strip()
        try:
            env[name] = eval(expr, {}, dict(env))
        except Exception:
            pass
    return env


class NS:
    def __init__(self, d):
        self.__dict__.update(d)


def haxe_literal(text, env):
    """Evaluate a Haxe array/object literal as Python."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            q = c
            j = i + 1
            buf = []
            while j < n and text[j] != q:
                if text[j] == '\\':
                    nx = text[j + 1]
                    buf.append({'n': '\n', 't': '\t', 'r': '\r', '"': '"', "'": "'", '\\': '\\'}.get(nx, nx))
                    j += 2
                    continue
                buf.append(text[j])
                j += 1
            out.append(repr(''.join(buf)))
            i = j + 1
            continue
        m = re.match(r'([A-Za-z_]\w*)\s*:(?!:)', text[i:])
        if m and (not out or out[-1].rstrip()[-1:] in ('{', ',', '')):
            out.append(repr(m.group(1)) + ':')
            i += m.end()
            continue
        out.append(c)
        i += 1
    py = ''.join(out)
    py = re.sub(r'\bnull\b', 'None', py)
    py = re.sub(r'\btrue\b', 'True', py)
    py = re.sub(r'\bfalse\b', 'False', py)
    return eval(py, {'__builtins__': {}}, env)


def find_literal(src, start_pat):
    m = re.search(start_pat, src)
    if not m:
        raise ValueError('pattern not found: ' + start_pat)
    i = m.end()
    while src[i] not in '[{':
        i += 1
    depth = 0
    j = i
    in_str = None
    while True:
        c = src[j]
        if in_str:
            if c == '\\':
                j += 2
                continue
            if c == in_str:
                in_str = None
        elif c in '"\'':
            in_str = c
        elif c in '[{':
            depth += 1
        elif c in ']}':
            depth -= 1
            if depth == 0:
                return src[i:j + 1]
        j += 1


def old_random(seed):
    """mt.OldRandom with Flash 8 double semantics (see src/mt/Random.h)."""
    def to_int32(d):
        return int(math.fmod(math.trunc(d), 4294967296.0)) & 0xFFFFFFFF
    prec = 55
    vals = []
    s = seed
    for _ in range(prec):
        s = to_int32(math.trunc(float(s) * 1103515245.0)) & 0x3FFFFFFF
        s = (s + 12345) & 0x3FFFFFFF
        sbig = s & 0x3FFF0000
        s = to_int32(math.trunc(float(s) * 1103515245.0)) & 0x3FFFFFFF
        s = (s + 12345) & 0x3FFFFFFF
        vals.append((s >> 16) | sbig)
    state = {'idx': 0}

    def nxt():
        state['idx'] = (state['idx'] + 1) % prec
        idx = state['idx']
        v = (vals[(idx + 24) % prec] + idx) & 0x3FFFFFFF
        vals[idx] = v
        return v
    return nxt


def main():
    base, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    mi_src = strip_comments(read(os.path.join(base, 'com/MissionInfo.hx')))
    zi_src = strip_comments(read(os.path.join(base, 'com/ZoneInfo.hx')))
    si_src = strip_comments(read(os.path.join(base, 'com/ShopInfo.hx')))

    zc = constants(zi_src)
    sc = constants(si_src)
    mc = constants(mi_src)
    env = dict(mc)
    env['ZoneInfo'] = NS(zc)
    env['ShopInfo'] = NS(sc)
    env['MissionInfo'] = NS(mc)

    # real planet list lives in initFonction's "#if web" branch
    web = zi_src[zi_src.index('static function initFonction'):]
    zones = haxe_literal(find_literal(web, r'#if web\s*var a\s*='), {})
    holes = haxe_literal(find_literal(zi_src, r'static var holes\s*='), {})

    missions = haxe_literal(find_literal(mi_src, r'static var LIST\s*:\s*Array<Mission>\s*='), env)
    webi = mi_src[mi_src.index('static function initFonction'):]
    items = haxe_literal(find_literal(webi, r'#if web\s*var a\s*='), env)
    web_end = webi.index('ITEMS = a')
    pos = 0
    while True:  # "a = a.concat([...])" continuations
        m = re.search(r'a\s*=\s*a\.concat\(', webi[pos:web_end])
        if not m:
            break
        lit = find_literal(webi[pos + m.start():], r'concat\(')
        items += haxe_literal(lit, env)
        pos += m.end()

    # missiles are generated procedurally after the item list (same code as MissionInfo.initFonction)
    nxt = old_random(2657)
    rnd = lambda m: nxt() % m
    max_ = mc['MISSILE_MAX'] - 1
    a = rnd(6280) / 1000
    mi12 = missions[12]
    for i in range(max_):
        c = i / max_
        idx = mc['MISSILE'] + (i + 1)
        ray = 9 + (c ** 2) * 500
        a += 1 + rnd(3000) / 1000
        x = int(math.cos(a) * ray)  # Std.int truncates toward zero
        y = int(math.sin(a) * ray)
        items.insert(idx, {'x': x, 'y': y, 'fam': 1})
        mi12['endItem'].append([idx, mc['SURPRISE']])

    shop = haxe_literal(find_literal(si_src, r'static var ITEMS\s*='), {})

    data = {
        'zones': zones,
        'holes': holes,
        'missions': missions,
        'items': items,
        'shop': shop,
        'constants': {'mission': mc, 'zone': zc, 'shop': sc},
    }
    with open(os.path.join(out, 'game_data.json'), 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=0)
    print('game_data.json: %d zones, %d missions, %d items, %d shop items' %
          (len(zones), len(missions), len(items), len(shop)))

    for lang in ('En', 'Fr', 'De', 'Es'):
        src = strip_comments(read(os.path.join(base, 'client/src/Text%s.hx' % lang)))
        texts = {}
        for m in re.finditer(r'public\s+static\s+var\s+([A-Z_0-9]+)\s*=', src):
            name = m.group(1)
            rest = src[m.end():].lstrip()
            if rest[0] in '[{':
                lit = find_literal(src[m.end() - 1:], r'=')
            else:
                # string (possibly concatenated)
                end = re.search(r';\s*(\n|$)', rest)
                lit = rest[:end.start()]
            try:
                v = haxe_literal(lit, {})
                if isinstance(v, tuple):
                    v = list(v)
                texts[name] = v
            except Exception as e:
                print('  %s.%s: skipped (%s)' % (lang, name, e))
        with open(os.path.join(out, 'text_%s.json' % lang.lower()), 'w', encoding='utf-8') as f:
            json.dump(texts, f, ensure_ascii=False, indent=0)
        print('text_%s.json: %d entries' % (lang.lower(), len(texts)))


    # website texts: the mission feed (mission_name_N / mission_desc_N / mission_end_N), vignette names
    swap = {'\u2019': "'", '\u2018': "'", '\u00b4': "'", '\u00ab': '"', '\u00bb': '"', '\u00a0': ' ',
            '\u00b0': 'o', '\u2026': '...'}
    for lang in ('en', 'fr', 'de', 'es'):
        path = os.path.join(base, 'web/xml/%s/texts.xml' % lang)
        if not os.path.exists(path):
            continue
        src = re.sub(r'<!--.*?-->', '', read(path), flags=re.S)
        site = {}
        for m in re.finditer(r'<t id="([^"]+)">(.*?)</t>', src, re.S):
            key, val = m.group(1), m.group(2)
            if not (key.startswith('mission_') or key.startswith('vig') or key.startswith('date_')):
                continue
            val = re.sub(r'\s+', ' ', val).strip()
            val = re.sub(r'>\s+<', '><', val)
            for a, b in swap.items():
                val = val.replace(a, b)
            site[key] = val
        with open(os.path.join(out, 'site_%s.json' % lang), 'w', encoding='utf-8') as f:
            json.dump(site, f, ensure_ascii=False, indent=0)
        print('site_%s.json: %d entries' % (lang, len(site)))


if __name__ == '__main__':
    main()
