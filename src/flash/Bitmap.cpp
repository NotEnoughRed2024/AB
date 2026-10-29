// flash.display.BitmapData on top of an SDL render-target texture.
#include <algorithm>
#include <cmath>

#include "Display.h"
#include "Filters.h"
#include "Stage.h"

namespace flash {

static constexpr double BITMAP_RES = 2.0;  // texture pixels per logical pixel (matches the atlas scale)

BitmapData::BitmapData(Stage* s, int w, int h, bool tr, unsigned fill, bool sft)
    : width(w), height(h), transparent(tr), res(BITMAP_RES), stage(s) {
    soft = sft;
    if (soft) {
        res = 1;
        tw = std::max(1, w);
        th = std::max(1, h);
        cpu.assign((size_t)tw * th, transparent ? fill : (fill | 0xFF000000u));
        return;
    }
    tw = std::max(1, (int)std::ceil(w * res));
    th = std::max(1, (int)std::ceil(h * res));
    texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, tw, th);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    Rect r;
    r.add(0, 0);
    r.add(w, h);
    fillRect(r, transparent ? fill : (fill | 0xFF000000u));
}

BitmapData::~BitmapData() { dispose(); }

void BitmapData::dispose() {
    if (texture) SDL_DestroyTexture(texture);
    texture = nullptr;
}

struct TargetScope {
    Stage* s;
    SDL_Texture* prev;
    int pw, ph;
    Matrix pview;
    TargetScope(Stage* st, SDL_Texture* t, int w, int h) : s(st) {
        prev = s->currentTarget;
        pw = s->targetW;
        ph = s->targetH;
        pview = s->view;
        SDL_SetRenderTarget(s->renderer, t);
        s->currentTarget = t;
        s->targetW = w;
        s->targetH = h;
    }
    ~TargetScope() {
        SDL_SetRenderTarget(s->renderer, prev);
        s->currentTarget = prev;
        s->targetW = pw;
        s->targetH = ph;
        s->view = pview;
    }
};

void BitmapData::fillRect(Rect r, unsigned argb) {
    if (soft) {
        if (!transparent) argb |= 0xFF000000u;
        for (int y = std::max(0, (int)r.yMin); y < std::min(th, (int)std::ceil(r.yMax)); y++)
            for (int x = std::max(0, (int)r.xMin); x < std::min(tw, (int)std::ceil(r.xMax)); x++) cpu[(size_t)y * tw + x] = argb;
        return;
    }
    if (!texture) return;
    TargetScope ts(stage, texture, tw, th);
    unsigned a = transparent ? (argb >> 24) : 255;
    // premultiplied storage for transparent bitmaps
    double k = transparent ? a / 255.0 : 1.0;
    SDL_SetRenderDrawColor(stage->renderer, (Uint8)(((argb >> 16) & 255) * k), (Uint8)(((argb >> 8) & 255) * k),
                           (Uint8)((argb & 255) * k), (Uint8)a);
    SDL_SetRenderDrawBlendMode(stage->renderer, SDL_BLENDMODE_NONE);
    SDL_FRect fr{(float)(r.xMin * res), (float)(r.yMin * res), (float)(r.width() * res), (float)(r.height() * res)};
    SDL_RenderFillRect(stage->renderer, &fr);
}

// ---- CPU rasteriser for soft bitmaps (4x4 supersampling like Flash's "high" quality)
static bool insidePolys(const std::vector<std::vector<double>>& polys, double x, double y) {
    bool in = false;
    for (const auto& p : polys) {
        size_t n = p.size() / 2;
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            double xi = p[2 * i], yi = p[2 * i + 1], xj = p[2 * j], yj = p[2 * j + 1];
            if (((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) in = !in;
        }
    }
    return in;
}

static void softShape(BitmapData* bd, const std::vector<VecFill>& fills, const Matrix& m, const ColorTransform& ct,
                      Blend blend) {
    Matrix inv = m.inverted();
    for (const auto& f : fills) {
        Rect b = m.transformRect(f.bounds);
        int x0 = std::max(0, (int)std::floor(b.xMin)), x1 = std::min(bd->width, (int)std::ceil(b.xMax) + 1);
        int y0 = std::max(0, (int)std::floor(b.yMin)), y1 = std::min(bd->height, (int)std::ceil(b.yMax) + 1);
        for (int py = y0; py < y1; py++)
            for (int px = x0; px < x1; px++) {
                if (!bd->softMask.empty() && !bd->softMask[(size_t)py * bd->width + px]) continue;
                double acc[4] = {0, 0, 0, 0};
                int hits = 0;
                for (int sy = 0; sy < 4; sy++)
                    for (int sx = 0; sx < 4; sx++) {
                        Point q = inv.apply(px + (sx + 0.5) / 4.0, py + (sy + 0.5) / 4.0);
                        if (!insidePolys(f.polys, q.x, q.y)) continue;
                        double c[4];
                        f.paint.colorAt(q.x, q.y, c);
                        double r = c[0], g = c[1], bb = c[2], a = c[3];
                        ct.applyTo(r, g, bb, a);
                        a = std::clamp(a, 0.0, 255.0);
                        acc[0] += std::clamp(r, 0.0, 255.0) * a;
                        acc[1] += std::clamp(g, 0.0, 255.0) * a;
                        acc[2] += std::clamp(bb, 0.0, 255.0) * a;
                        acc[3] += a;
                        hits++;
                    }
                if (!hits) continue;
                double sa = acc[3] / 16.0 / 255.0;  // coverage-weighted alpha 0..1
                if (sa <= 0) continue;
                double sr = acc[0] / acc[3], sg = acc[1] / acc[3], sb = acc[2] / acc[3];
                uint32_t& d = bd->cpu[(size_t)py * bd->width + px];
                double dr = (d >> 16) & 255, dg = (d >> 8) & 255, db = d & 255, da = (d >> 24) / 255.0;
                double nr, ng, nb, na;
                switch (blend) {
                    case Blend::Add:
                        nr = dr + sr * sa; ng = dg + sg * sa; nb = db + sb * sa; na = std::min(1.0, da + sa);
                        break;
                    case Blend::Subtract:
                        nr = dr - sr * sa; ng = dg - sg * sa; nb = db - sb * sa; na = da;
                        break;
                    default: {
                        na = sa + da * (1 - sa);
                        double k = na > 0 ? 1.0 / na : 0;
                        nr = (sr * sa + dr * da * (1 - sa)) * k;
                        ng = (sg * sa + dg * da * (1 - sa)) * k;
                        nb = (sb * sa + db * da * (1 - sa)) * k;
                        if (!bd->transparent) { nr = sr * sa + dr * (1 - sa); ng = sg * sa + dg * (1 - sa); nb = sb * sa + db * (1 - sa); }
                    }
                }
                if (!bd->transparent) na = 1;
                d = ((uint32_t)std::lround(std::clamp(na, 0.0, 1.0) * 255) << 24) |
                    ((uint32_t)std::lround(std::clamp(nr, 0.0, 255.0)) << 16) |
                    ((uint32_t)std::lround(std::clamp(ng, 0.0, 255.0)) << 8) | (uint32_t)std::lround(std::clamp(nb, 0.0, 255.0));
            }
    }
}

static void softDraw(BitmapData* bd, DisplayObject* o, const Matrix& m, const ColorTransform& ct, Blend blend, bool root) {
    if (!o->visible && !root) return;
    Matrix lm = root ? m : m * o->matrix();
    ColorTransform lct = root ? ct : o->colorTransform().concat(ct);
    Blend b = (!root && o->blendMode != Blend::Normal) ? o->blendMode : blend;
    if (o->kind == DisplayObject::KShape) {
        auto* sh = static_cast<Shape*>(o);
        const auto* v = sh->lib->vectorShape(sh->charId);
        if (v) softShape(bd, *v, lm, lct, b);
        else SDL_Log("soft BitmapData.draw: shape %d has no vector data (add it to --vector)", sh->charId);
    } else if (o->kind == DisplayObject::KClip) {
        for (auto& [d, c] : static_cast<MovieClip*>(o)->children) softDraw(bd, c.get(), lm, lct, b, false);
    }
}

void BitmapData::draw(DisplayObject* src, const Matrix& m, const ColorTransform* ct, Blend blend) {
    if (soft && src) {
        softDraw(this, src, m, ct ? *ct : ColorTransform(), blend, true);
        return;
    }
    if (!texture || !src) return;
    TargetScope ts(stage, texture, tw, th);
    // Flash ignores the source's own matrix, colour transform and blend mode for draw()
    double sx = src->x, sy = src->y, sxs = src->xscale, sys = src->yscale, sr = src->rotation, sk = src->skew;
    double sa = src->alpha;
    ColorTransform sc = src->color;
    Blend sb = src->blendMode;
    bool sv = src->visible;
    src->x = src->y = 0;
    src->xscale = src->yscale = 100;
    src->rotation = src->skew = 0;
    src->alpha = 100;
    src->color = ColorTransform();
    src->blendMode = Blend::Normal;
    src->visible = true;
    Matrix base = Matrix::make(res, 0, 0, res, 0, 0) * m;
    stage->renderObject(src, base, ct ? *ct : ColorTransform(), blend, transparent);
    src->x = sx; src->y = sy; src->xscale = sxs; src->yscale = sys; src->rotation = sr; src->skew = sk;
    src->alpha = sa; src->color = sc; src->blendMode = sb; src->visible = sv;
}

void BitmapData::draw(BitmapData* src, const Matrix& m, const ColorTransform* ct, Blend blend) {
    if (!texture || !src || !src->texture) return;
    TargetScope ts(stage, texture, tw, th);
    Matrix base = Matrix::make(res, 0, 0, res, 0, 0) * m * Matrix::make(1.0 / src->res, 0, 0, 1.0 / src->res, 0, 0);
    stage->drawTexture(src->texture, src->tw, src->th, base, ct ? *ct : ColorTransform(), blend, src->transparent);
}

std::vector<uint32_t> BitmapData::readPixels() {
    if (soft) return cpu;
    std::vector<uint32_t> out((size_t)tw * th, 0);
    if (!texture) return out;
    TargetScope ts(stage, texture, tw, th);
    SDL_Surface* s = SDL_RenderReadPixels(stage->renderer, nullptr);
    if (!s) return out;
    SDL_Surface* cs = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ARGB8888);
    SDL_DestroySurface(s);
    if (!cs) return out;
    for (int y = 0; y < std::min(th, cs->h); y++)
        SDL_memcpy(&out[(size_t)y * tw], (Uint8*)cs->pixels + y * cs->pitch, (size_t)std::min(tw, cs->w) * 4);
    SDL_DestroySurface(cs);
    return out;
}

void BitmapData::writePixels(const std::vector<uint32_t>& px) {
    if (soft) {
        cpu = px;
        return;
    }
    if (!texture) return;
    SDL_Texture* up = TexturePool::get().acquire(stage->renderer, tw, th, SDL_TEXTUREACCESS_STREAMING);
    float uw, uh;
    SDL_GetTextureSize(up, &uw, &uh);
    std::vector<uint32_t> buf((size_t)uw * uh, 0);
    for (int y = 0; y < th; y++) SDL_memcpy(&buf[(size_t)y * (int)uw], &px[(size_t)y * tw], (size_t)tw * 4);
    SDL_UpdateTexture(up, nullptr, buf.data(), (int)uw * 4);
    TargetScope ts(stage, texture, tw, th);
    SDL_SetTextureBlendMode(up, SDL_BLENDMODE_NONE);
    SDL_FRect src{0, 0, (float)tw, (float)th};
    SDL_RenderTexture(stage->renderer, up, &src, nullptr);
}

// replace the bitmap's pixels with a pooled texture's (0,0,tw,th)
static void copyInto(Stage* st, SDL_Texture* dst, SDL_Texture* src, int tw, int th) {
    TargetScope ts(st, dst, tw, th);
    SDL_SetTextureBlendMode(src, SDL_BLENDMODE_NONE);
    SDL_FRect r{0, 0, (float)tw, (float)th};
    SDL_RenderTexture(st->renderer, src, &r, &r);
}

void BitmapData::applyBlur(double bx, double by, int quality) {
    if (!soft && texture && stage->customBlend && !SDL_getenv("AB_CPU_FILTERS")) {
        // on the GPU: reading the pixels back every frame (the plasma trails, explosions) stalled rendering
        double rx = std::max(0.0, bx * res - 1) / 2, ry = std::max(0.0, by * res - 1) / 2;
        SDL_Texture* cur = texture;
        for (int p = 0; p < std::max(1, quality); p++) {
            cur = stage->blurTexture(cur, tw, th, rx, true);
            cur = stage->blurTexture(cur, tw, th, ry, false);
        }
        if (cur != texture) copyInto(stage, texture, cur, tw, th);
        return;
    }
    PixelBuffer pb;
    pb.w = tw;
    pb.h = th;
    pb.px = readPixels();
    boxBlur(pb, bx * res, by * res, quality);
    writePixels(pb.px);
}

void BitmapData::applyFilter(const Filter& f) {
    if (f.type == Filter::Blur) {
        applyBlur(f.blurX, f.blurY, f.quality);
        return;
    }
    PixelBuffer pb;
    pb.w = tw;
    pb.h = th;
    pb.px = readPixels();
    applyFilterCPU(pb, f, res);
    writePixels(pb.px);
}

void BitmapData::colorTransform(const ColorTransform& ct) {
    if (!soft && texture && stage->customBlend && transparent && !SDL_getenv("AB_CPU_FILTERS")) {
        // On the GPU, with premultiplied pixels: the multipliers are exact; offsets are applied to the
        // premultiplied values (exact on opaque pixels, close on the fading trails this is used for), and
        // positive colour offsets only where there is alpha, as in Flash.
        SDL_Renderer* r = stage->renderer;
        SDL_Texture* tmp = TexturePool::get().acquire(r, tw, th, SDL_TEXTUREACCESS_TARGET);
        {
            TargetScope ts(stage, tmp, tw, th);
            SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
            SDL_RenderClear(r);
            auto c01 = [](double v) { return (float)std::clamp(v, 0.0, 1.0); };
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
            SDL_SetTextureColorModFloat(texture, c01(ct.rm * ct.am), c01(ct.gm * ct.am), c01(ct.bm * ct.am));
            SDL_SetTextureAlphaModFloat(texture, c01(ct.am));
            SDL_FRect all{0, 0, (float)tw, (float)th};
            SDL_RenderTexture(r, texture, &all, &all);
            SDL_SetTextureColorModFloat(texture, 1, 1, 1);
            SDL_SetTextureAlphaModFloat(texture, 1);
            auto rect = [&](double cr, double cg, double cb, double ca, SDL_BlendMode m) {
                SDL_SetRenderDrawBlendMode(r, m);
                SDL_SetRenderDrawColorFloat(r, (float)cr, (float)cg, (float)cb, (float)ca);
                SDL_RenderFillRect(r, &all);
            };
            double pr = std::max(0.0, ct.ra) / 255, pg = std::max(0.0, ct.ga) / 255, pb = std::max(0.0, ct.ba) / 255;
            if (pr + pg + pb > 0)
                rect(pr, pg, pb, 0,
                     SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_DST_ALPHA, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD,
                                                SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD));
            if (ct.aa > 0)
                rect(0, 0, 0, ct.aa / 255,
                     SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD,
                                                SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD));
            double nr = std::max(0.0, -ct.ra) / 255, ng = std::max(0.0, -ct.ga) / 255, nb = std::max(0.0, -ct.ba) / 255,
                   na = std::max(0.0, -ct.aa) / 255;
            if (nr + ng + nb + na > 0)
                rect(std::max(nr, na), std::max(ng, na), std::max(nb, na), na,
                     SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_REV_SUBTRACT,
                                                SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE,
                                                SDL_BLENDOPERATION_REV_SUBTRACT));
        }
        copyInto(stage, texture, tmp, tw, th);
        return;
    }
    auto px = readPixels();
    for (auto& p : px) {
        double a = p >> 24, r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
        if (transparent) {
            if (a <= 0) continue;
            r = r * 255 / a; g = g * 255 / a; b = b * 255 / a;
        }
        r = std::clamp(r * ct.rm + ct.ra, 0.0, 255.0);
        g = std::clamp(g * ct.gm + ct.ga, 0.0, 255.0);
        b = std::clamp(b * ct.bm + ct.ba, 0.0, 255.0);
        a = transparent ? std::clamp(a * ct.am + ct.aa, 0.0, 255.0) : 255.0;
        double k = transparent ? a / 255.0 : 1.0;
        p = ((uint32_t)a << 24) | ((uint32_t)(r * k) << 16) | ((uint32_t)(g * k) << 8) | (uint32_t)(b * k);
    }
    writePixels(px);
}

// --- Perlin noise (Flash-style fractal noise; not bit-exact with Flash, only used for textures)
static double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static double lerp(double a, double b, double t) { return a + (b - a) * t; }

void BitmapData::perlinNoise(double baseX, double baseY, int octaves, int seed, bool, bool fractal, int channels,
                             bool grey) {
    int perm[512];
    uint32_t s = (uint32_t)seed * 2654435761u + 1;
    int p[256];
    for (int i = 0; i < 256; i++) p[i] = i;
    for (int i = 255; i > 0; i--) {
        s = s * 1103515245u + 12345u;
        int j = (int)((s >> 16) % (uint32_t)(i + 1));
        std::swap(p[i], p[j]);
    }
    for (int i = 0; i < 512; i++) perm[i] = p[i & 255];
    auto grad = [&](int h, double x, double y) {
        switch (h & 3) {
            case 0: return x + y;
            case 1: return -x + y;
            case 2: return x - y;
            default: return -x - y;
        }
    };
    auto noise = [&](double x, double y, int ch) {
        int X = (int)std::floor(x) & 255, Y = (int)std::floor(y) & 255;
        x -= std::floor(x);
        y -= std::floor(y);
        double u = fade(x), v = fade(y);
        int o = ch * 37;
        int aa = perm[perm[(X + o) & 255] + Y], ab = perm[perm[(X + o) & 255] + Y + 1];
        int ba = perm[perm[(X + 1 + o) & 255] + Y], bb = perm[perm[(X + 1 + o) & 255] + Y + 1];
        return lerp(lerp(grad(aa, x, y), grad(ba, x - 1, y), u), lerp(grad(ab, x, y - 1), grad(bb, x - 1, y - 1), u), v);
    };
    std::vector<uint32_t> px((size_t)tw * th);
    for (int y = 0; y < th; y++)
        for (int x = 0; x < tw; x++) {
            double lx = x / res, ly = y / res;
            int out[3] = {0, 0, 0};
            for (int ch = 0; ch < (grey ? 1 : 3); ch++) {
                double sum = 0, amp = 1, fx = lx / baseX, fy = ly / baseY, tot = 0;
                for (int o = 0; o < std::max(1, octaves); o++) {
                    double n = noise(fx, fy, ch + o * 3);
                    sum += (fractal ? n : std::fabs(n)) * amp;
                    tot += amp;
                    amp *= 0.5;
                    fx *= 2;
                    fy *= 2;
                }
                double v = fractal ? (sum / tot * 0.5 + 0.5) : (sum / tot);
                out[ch] = (int)std::clamp(v * 255.0, 0.0, 255.0);
            }
            if (grey) out[1] = out[2] = out[0];
            px[(size_t)y * tw + x] = 0xFF000000u | (out[0] << 16) | (out[1] << 8) | out[2];
        }
    (void)channels;
    writePixels(px);
}

unsigned BitmapData::getPixel32(int x, int y) {
    if (soft) {
        if (x < 0 || y < 0 || x >= tw || y >= th) return 0;
        return cpu[(size_t)y * tw + x];
    }
    auto px = readPixels();
    int ix = (int)(x * res), iy = (int)(y * res);
    if (ix < 0 || iy < 0 || ix >= tw || iy >= th) return 0;
    return px[(size_t)iy * tw + ix];
}

void BitmapData::setPixel32(int x, int y, unsigned argb) {
    Rect r;
    r.add(x, y);
    r.add(x + 1, y + 1);
    fillRect(r, argb);
}

}  // namespace flash
