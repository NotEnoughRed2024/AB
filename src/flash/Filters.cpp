#include "Filters.h"

#include <algorithm>
#include <cmath>

namespace flash {

static inline uint32_t A(uint32_t p) { return p >> 24; }
static inline uint32_t R(uint32_t p) { return (p >> 16) & 255; }
static inline uint32_t G(uint32_t p) { return (p >> 8) & 255; }
static inline uint32_t B(uint32_t p) { return p & 255; }
static inline uint32_t pack(int a, int r, int g, int b) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

// One horizontal box-blur pass over 4 channels with a window of `radius*2+1`.
static void blurLine(const uint32_t* in, uint32_t* out, int n, int stride, int radius) {
    if (radius <= 0) {
        for (int i = 0; i < n; i++) out[i * stride] = in[i * stride];
        return;
    }
    int win = radius * 2 + 1;
    int sa = 0, sr = 0, sg = 0, sb = 0;
    auto at = [&](int i) -> uint32_t { return (i < 0 || i >= n) ? 0u : in[i * stride]; };
    for (int i = -radius; i <= radius; i++) {
        uint32_t p = at(i);
        sa += A(p); sr += R(p); sg += G(p); sb += B(p);
    }
    for (int i = 0; i < n; i++) {
        out[i * stride] = pack(sa / win, sr / win, sg / win, sb / win);
        uint32_t pin = at(i + radius + 1), pout = at(i - radius);
        sa += (int)A(pin) - (int)A(pout);
        sr += (int)R(pin) - (int)R(pout);
        sg += (int)G(pin) - (int)G(pout);
        sb += (int)B(pin) - (int)B(pout);
    }
}

void boxBlur(PixelBuffer& pb, double bx, double by, int passes) {
    // Flash: blurX is the box width; quality = number of passes
    int rx = (int)std::floor(std::max(0.0, bx - 1) / 2.0 + 0.5);
    int ry = (int)std::floor(std::max(0.0, by - 1) / 2.0 + 0.5);
    if (rx <= 0 && ry <= 0) return;
    std::vector<uint32_t> tmp(pb.px.size());
    for (int p = 0; p < std::max(1, passes); p++) {
        if (rx > 0) {
            for (int y = 0; y < pb.h; y++) blurLine(&pb.px[(size_t)y * pb.w], &tmp[(size_t)y * pb.w], pb.w, 1, rx);
            pb.px.swap(tmp);
        }
        if (ry > 0) {
            for (int x = 0; x < pb.w; x++) blurLine(&pb.px[x], &tmp[x], pb.h, pb.w, ry);
            pb.px.swap(tmp);
        }
    }
}

static void glowLike(PixelBuffer& pb, const Filter& f, double scale, double dx, double dy) {
    int w = pb.w, h = pb.h;
    // alpha mask of the source (optionally offset)
    PixelBuffer mask;
    mask.w = w;
    mask.h = h;
    mask.px.assign((size_t)w * h, 0);
    int ox = (int)std::lround(dx), oy = (int)std::lround(dy);
    for (int y = 0; y < h; y++) {
        int sy = y - oy;
        if (sy < 0 || sy >= h) continue;
        for (int x = 0; x < w; x++) {
            int sx = x - ox;
            if (sx < 0 || sx >= w) continue;
            uint32_t a = A(pb.px[(size_t)sy * w + sx]);
            if (f.inner) a = 255 - a;
            mask.px[(size_t)y * w + x] = a << 24;
        }
    }
    if (f.inner) {
        // outside the image counts as transparent -> inner glow from the edges
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int sx = x - ox, sy = y - oy;
                if (sx < 0 || sy < 0 || sx >= w || sy >= h) mask.px[(size_t)y * w + x] = 255u << 24;
            }
    }
    boxBlur(mask, f.blurX * scale, f.blurY * scale, std::max(1, f.quality));
    int cr = (f.color >> 16) & 255, cg = (f.color >> 8) & 255, cb = f.color & 255;
    for (size_t i = 0; i < pb.px.size(); i++) {
        uint32_t src = pb.px[i];
        double ga = std::min(1.0, (A(mask.px[i]) / 255.0) * f.strength) * f.alpha;
        double sa = A(src) / 255.0;
        double outA, outR, outG, outB;
        if (f.inner) {
            // glow inside the shape: composite glow over source, masked by source alpha
            double gi = ga * sa;
            outR = R(src) * (1 - gi) + cr * gi;
            outG = G(src) * (1 - gi) + cg * gi;
            outB = B(src) * (1 - gi) + cb * gi;
            outA = A(src);
            if (f.knockout) {
                outR = cr * gi; outG = cg * gi; outB = cb * gi; outA = gi * 255;
            }
        } else {
            // source over glow
            double gpR = cr * ga, gpG = cg * ga, gpB = cb * ga;  // premultiplied glow
            if (f.knockout) {
                double k = 1 - sa;
                outR = gpR * k; outG = gpG * k; outB = gpB * k; outA = ga * k * 255;
            } else {
                outR = R(src) + gpR * (1 - sa);
                outG = G(src) + gpG * (1 - sa);
                outB = B(src) + gpB * (1 - sa);
                outA = A(src) + ga * 255 * (1 - sa);
            }
        }
        pb.px[i] = pack((int)std::min(255.0, outA), (int)std::min(255.0, outR), (int)std::min(255.0, outG),
                        (int)std::min(255.0, outB));
    }
}

static void colorMatrix(PixelBuffer& pb, const double* m) {
    for (auto& p : pb.px) {
        double a = A(p);
        if (a <= 0 && m[19] <= 0) continue;
        // un-premultiply
        double r = a > 0 ? R(p) * 255.0 / a : 0, g = a > 0 ? G(p) * 255.0 / a : 0, b = a > 0 ? B(p) * 255.0 / a : 0;
        double nr = m[0] * r + m[1] * g + m[2] * b + m[3] * a + m[4];
        double ng = m[5] * r + m[6] * g + m[7] * b + m[8] * a + m[9];
        double nb = m[10] * r + m[11] * g + m[12] * b + m[13] * a + m[14];
        double na = m[15] * r + m[16] * g + m[17] * b + m[18] * a + m[19];
        na = std::clamp(na, 0.0, 255.0);
        double k = na / 255.0;
        p = pack((int)na, (int)(std::clamp(nr, 0.0, 255.0) * k), (int)(std::clamp(ng, 0.0, 255.0) * k),
                 (int)(std::clamp(nb, 0.0, 255.0) * k));
    }
}

void applyFilterCPU(PixelBuffer& pb, const Filter& f, double scale) {
    switch (f.type) {
        case Filter::Blur:
            boxBlur(pb, f.blurX * scale, f.blurY * scale, std::max(1, f.quality));
            break;
        case Filter::Glow:
            glowLike(pb, f, scale, 0, 0);
            break;
        case Filter::Shadow:
            glowLike(pb, f, scale, std::cos(f.angle) * f.distance * scale, std::sin(f.angle) * f.distance * scale);
            break;
        case Filter::ColorMatrix:
            colorMatrix(pb, f.matrix);
            break;
    }
}

// ------------------------------------------------------------------ texture pool

TexturePool& TexturePool::get() {
    static TexturePool p;
    return p;
}

SDL_Texture* TexturePool::acquire(SDL_Renderer* r, int w, int h, SDL_TextureAccess access) {
    // round up so similar sizes share textures
    int rw = (w + 31) & ~31, rh = (h + 31) & ~31;
    Key k{rw, rh, (int)access};
    SDL_Texture* t = nullptr;
    auto it = free_.find(k);
    if (it != free_.end() && !it->second.empty()) {
        t = it->second.back();
        it->second.pop_back();
    } else {
        t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, access, rw, rh);
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    }
    used_.push_back({k, t});
    // callers address the texture as w x h: report that through a sub-rect convention -> keep exact size
    // simplest: textures are exact-size when access is streaming (UpdateTexture needs it)
    return t;
}

void TexturePool::endFrame() {
    for (auto& [k, t] : used_) free_[k].push_back(t);
    used_.clear();
}

void TexturePool::clear() {
    for (auto& [k, v] : free_)
        for (auto* t : v) SDL_DestroyTexture(t);
    for (auto& [k, t] : used_) SDL_DestroyTexture(t);
    free_.clear();
    used_.clear();
}

}  // namespace flash
