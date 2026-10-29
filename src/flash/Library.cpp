#include "Library.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include "json.hpp"
#include "stb_image.h"

using json = nlohmann::json;

namespace flash {

Blend blendFromString(const std::string& s0) {
    std::string s;
    for (char ch : s0) s += (char)std::tolower((unsigned char)ch);
    if (s == "add") return Blend::Add;
    if (s == "subtract") return Blend::Subtract;
    if (s == "multiply") return Blend::Multiply;
    if (s == "screen") return Blend::Screen;
    if (s == "lighten") return Blend::Lighten;
    if (s == "darken") return Blend::Darken;
    if (s == "difference") return Blend::Difference;
    if (s == "overlay") return Blend::Overlay;
    if (s == "hardlight") return Blend::Hardlight;
    if (s == "invert") return Blend::Invert;
    if (s == "alpha") return Blend::Alpha;
    if (s == "erase") return Blend::Erase;
    if (s == "layer") return Blend::Layer;
    return Blend::Normal;
}

static unsigned rgbOf(const json& c) { return ((unsigned)c[0] << 16) | ((unsigned)c[1] << 8) | (unsigned)c[2]; }

static Filter parseFilter(const json& f) {
    Filter out;
    std::string t = f["t"];
    out.blurX = f.value("bx", 0.0);
    out.blurY = f.value("by", 0.0);
    out.strength = f.value("s", 1.0);
    out.inner = f.value("inner", false);
    out.knockout = f.value("knockout", false);
    if (f.contains("c")) {
        out.color = rgbOf(f["c"]);
        out.alpha = (double)f["c"][3] / 255.0;
    }
    if (t == "glow") out.type = Filter::Glow;
    else if (t == "blur") out.type = Filter::Blur;
    else if (t == "shadow") {
        out.type = Filter::Shadow;
        out.angle = f.value("angle", 0.0);
        out.distance = f.value("dist", 0.0);
    } else if (t == "colorMatrix") {
        out.type = Filter::ColorMatrix;
        for (int i = 0; i < 20; i++) out.matrix[i] = f["m"][i];
    }
    return out;
}

static std::vector<Frame> parseFrames(const json& frames) {
    std::vector<Frame> out;
    out.reserve(frames.size());
    for (const auto& fr : frames) {
        Frame f;
        for (const auto& p : fr["dl"]) {
            Placement pl;
            pl.character = p["c"];
            pl.instance = p["i"];
            pl.depth = p["d"];
            const auto& m = p["m"];
            pl.matrix = Matrix::make(m[0], m[1], m[2], m[3], m[4], m[5]);
            if (p.contains("cx")) {
                const auto& c = p["cx"];
                pl.hasColor = true;
                pl.color = ColorTransform(c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]);
            }
            if (p.contains("n")) pl.name = p["n"];
            if (p.contains("cd")) pl.clipDepth = p["cd"];
            if (p.contains("f"))
                for (const auto& fl : p["f"]) pl.filters.push_back(parseFilter(fl));
            if (p.contains("b")) pl.blend = blendFromString(p["b"]);
            f.displayList.push_back(std::move(pl));
        }
        if (fr.contains("a")) {
            for (const auto& a : fr["a"]) {
                std::string s = a;
                if (s == "stop") f.actions.push_back({FrameAction::Stop, 0});
                else if (s == "play") f.actions.push_back({FrameAction::Play, 0});
                else if (s == "randomFrame") f.actions.push_back({FrameAction::RandomFrame, 0});
                else if (s == "removeSelf") f.actions.push_back({FrameAction::RemoveSelf, 0});
                else if (s == "callObjKill") f.actions.push_back({FrameAction::CallObjKill, 0});
                else if (s == "randomRotation") f.actions.push_back({FrameAction::RandomRotation, 0});
                else if (s == "counterParentRotation") f.actions.push_back({FrameAction::CounterParentRotation, 0});
                else if (s == "rotate2") f.actions.push_back({FrameAction::Rotate2, 0});
                else if (s == "gotoAndPlay1") f.actions.push_back({FrameAction::GotoAndPlay1, 0});
                else if (s.rfind("goto:", 0) == 0) f.actions.push_back({FrameAction::Goto, std::stoi(s.substr(5))});
            }
        }
        if (fr.contains("label")) f.label = fr["label"];
        out.push_back(std::move(f));
    }
    return out;
}

bool Library::load(SDL_Renderer* r, const std::string& dir, const std::string& nm) {
    renderer = r;
    name = nm;
    std::string path = dir + "/" + nm + ".json";
    size_t sz = 0;
    void* raw = SDL_LoadFile(path.c_str(), &sz);
    if (!raw) {
        SDL_Log("Library: cannot read %s: %s", path.c_str(), SDL_GetError());
        return false;
    }
    json j = json::parse((const char*)raw, (const char*)raw + sz);
    SDL_free(raw);

    scale = j.value("scale", 2.0);
    for (const auto& im : j["images"]) {
        Image i;
        i.page = im[0];
        i.x = im[1];
        i.y = im[2];
        i.w = im[3];
        i.h = im[4];
        i.ox = im[5];
        i.oy = im[6];
        images.push_back(i);
    }
    for (const auto& p : j["pages"]) {
        std::string file = dir + "/" + std::string(p);
        int w, h, n;
        unsigned char* px = stbi_load(file.c_str(), &w, &h, &n, 4);
        if (!px) {
            SDL_Log("Library: cannot load %s", file.c_str());
            return false;
        }
        SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, px, w * 4);
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
        SDL_DestroySurface(s);
        stbi_image_free(px);
        if (!t) {
            SDL_Log("Library: texture %s: %s", file.c_str(), SDL_GetError());
            return false;
        }
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
        pages.push_back(t);
    }
    silhouettes.assign(pages.size(), nullptr);

    for (auto it = j["chars"].begin(); it != j["chars"].end(); ++it) {
        int id = std::stoi(it.key());
        const json& c = it.value();
        Character ch;
        std::string t = c["type"];
        if (t == "shape") {
            ch.kind = Character::Shape;
            ch.image = c["image"];
        } else if (t == "bitmap") {
            ch.kind = Character::Bitmap;
            ch.image = c["image"];
            ch.bitmapW = c["width"];
            ch.bitmapH = c["height"];
        } else if (t == "sprite") {
            ch.kind = Character::Sprite;
            ch.frames = parseFrames(c["frames"]);
            for (auto l = c["labels"].begin(); l != c["labels"].end(); ++l) ch.labels[l.key()] = l.value();
        } else if (t == "edittext") {
            ch.kind = Character::EditText;
            auto& e = ch.edit;
            const auto& b = c["bounds"];
            e.bounds.empty = false;
            e.bounds.xMin = b[0];
            e.bounds.xMax = b[1];
            e.bounds.yMin = b[2];
            e.bounds.yMax = b[3];
            e.font = c.value("font", 0);
            e.height = c.value("height", 12.0);
            e.color = rgbOf(c["color"]);
            e.alpha = (double)c["color"][3] / 255.0;
            e.align = c.value("align", 0);
            e.leftMargin = c.value("leftMargin", 0.0);
            e.rightMargin = c.value("rightMargin", 0.0);
            e.indent = c.value("indent", 0.0);
            e.leading = c.value("leading", 0.0);
            e.wordWrap = c.value("wordWrap", false);
            e.multiline = c.value("multiline", false);
            e.html = c.value("html", false);
            e.outlines = c.value("outlines", false);
            e.autoSize = c.value("autoSize", false);
            e.border = c.value("border", false);
            e.text = c.value("text", std::string());
            e.variable = c.value("variable", std::string());
        } else if (t == "font") {
            ch.kind = Character::FontRef;
        }
        chars[id] = std::move(ch);
    }
    for (auto it = j["exports"].begin(); it != j["exports"].end(); ++it) exports[it.key()] = it.value();
    for (auto it = j["fonts"].begin(); it != j["fonts"].end(); ++it) {
        Font f;
        const json& fj = it.value();
        f.name = fj["name"];
        f.bold = fj.value("bold", false);
        f.italic = fj.value("italic", false);
        f.emPx = fj.value("emPx", 32.0);
        f.ascent = fj.value("ascent", 0.9);
        f.descent = fj.value("descent", 0.25);
        f.leading = fj.value("leading", 0.0);
        for (const auto& g : fj["glyphs"]) {
            Glyph gl;
            gl.code = g["code"];
            gl.advance = g["advance"];
            gl.image = g.value("image", -1);
            f.byCode[gl.code] = (int)f.glyphs.size();
            f.glyphs.push_back(gl);
        }
        fonts[std::stoi(it.key())] = std::move(f);
    }
    if (j.contains("root")) rootFrames = parseFrames(j["root"]);
    if (j.contains("vectors")) {
        for (auto it = j["vectors"].begin(); it != j["vectors"].end(); ++it) {
            std::vector<VecFill> fills;
            for (const auto& f : it.value()) {
                VecFill vf;
                const auto& p = f["paint"];
                std::string t = p["type"];
                if (t == "solid") {
                    vf.paint.type = VecPaint::Solid;
                    vf.paint.r = p["color"][0];
                    vf.paint.g = p["color"][1];
                    vf.paint.b = p["color"][2];
                    vf.paint.a = p["color"][3];
                } else {
                    vf.paint.type = (t == "linear") ? VecPaint::Linear : VecPaint::Radial;
                    const auto& m = p["matrix"];
                    vf.paint.matrix = Matrix::make(m[0], m[1], m[2], m[3], m[4], m[5]);
                    for (const auto& st : p["stops"])
                        vf.paint.stops.push_back({(double)st[0], {(double)st[1][0], (double)st[1][1], (double)st[1][2], (double)st[1][3]}});
                    vf.paint.spread = p.value("spread", 0);
                }
                for (const auto& poly : f["polys"]) {
                    std::vector<double> pts;
                    for (const auto& v : poly) pts.push_back(v);
                    for (size_t k = 0; k + 1 < pts.size(); k += 2) vf.bounds.add(pts[k], pts[k + 1]);
                    vf.polys.push_back(std::move(pts));
                }
                fills.push_back(std::move(vf));
            }
            vectors[std::stoi(it.key())] = std::move(fills);
        }
    }
    SDL_Log("Library %s: %zu images, %zu pages, %zu chars, %zu exports, %zu fonts", nm.c_str(), images.size(),
            pages.size(), chars.size(), exports.size(), fonts.size());
    return true;
}

void Library::unload() {
    for (auto* t : pages) SDL_DestroyTexture(t);
    for (auto* t : silhouettes)
        if (t) SDL_DestroyTexture(t);
    pages.clear();
    silhouettes.clear();
}

const Character* Library::character(int id) const {
    auto it = chars.find(id);
    return it == chars.end() ? nullptr : &it->second;
}

int Library::exportId(const std::string& linkage) const {
    auto it = exports.find(linkage);
    return it == exports.end() ? -1 : it->second;
}

const Font* Library::font(int id) const {
    auto it = fonts.find(id);
    return it == fonts.end() ? nullptr : &it->second;
}

const Font* Library::fontByName(const std::string& n, bool bold) const {
    // deterministic choice: matching style first, then the most glyphs, then the first defined
    // (gfx.swf embeds two "Verdana" fonts; the later one, 858, has narrower outlines)
    const Font* best = nullptr;
    int bestId = 0;
    auto score = [&](const Font& f) {
        return (f.bold == bold ? 1000000 : 0) + (SDL_strcasecmp(f.name.c_str(), n.c_str()) == 0 ? 100000 : 0) +
               (int)f.glyphs.size();
    };
    for (const auto& [id, f] : fonts) {
        if (SDL_strcasecmp(f.name.c_str(), n.c_str()) != 0 && !SDL_strcasestr(f.name.c_str(), n.c_str())) continue;
        if (!best || score(f) > score(*best) || (score(f) == score(*best) && id < bestId)) {
            best = &f;
            bestId = id;
        }
    }
    return best;
}

void VecPaint::colorAt(double x, double y, double out[4]) const {
    if (type == Solid || stops.empty()) {
        out[0] = r; out[1] = g; out[2] = b; out[3] = a;
        return;
    }
    Point p = matrix.inverted().apply(x, y);  // into gradient square (-819.2 .. 819.2)
    double t;
    if (type == Linear) t = (p.x / 819.2 + 1) * 0.5;
    else t = std::sqrt(p.x * p.x + p.y * p.y) / 819.2;
    if (spread == 1) {  // reflect
        t = std::fmod(std::fabs(t), 2.0);
        if (t > 1) t = 2 - t;
    } else if (spread == 2) {  // repeat
        t = t - std::floor(t);
    }
    t = std::clamp(t, 0.0, 1.0) * 255.0;
    if (t <= stops.front().first) {
        for (int i = 0; i < 4; i++) out[i] = stops.front().second[i];
        return;
    }
    for (size_t i = 1; i < stops.size(); i++) {
        if (t <= stops[i].first) {
            double span = stops[i].first - stops[i - 1].first;
            double k = span > 0 ? (t - stops[i - 1].first) / span : 0;
            for (int c = 0; c < 4; c++) out[c] = stops[i - 1].second[c] + (stops[i].second[c] - stops[i - 1].second[c]) * k;
            return;
        }
    }
    for (int i = 0; i < 4; i++) out[i] = stops.back().second[i];
}

Rect Library::imageBounds(int i) const {
    const Image& im = images[i];
    Rect r;
    r.add(-im.ox / scale, -im.oy / scale);
    r.add((im.w - im.ox) / scale, (im.h - im.oy) / scale);
    return r;
}

SDL_Texture* Library::silhouettePage(int i) {
    if (silhouettes[i]) return silhouettes[i];
    float w, h;
    SDL_GetTextureSize(pages[i], &w, &h);
    SDL_Texture* t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, (int)w, (int)h);
    if (!t) return pages[i];
    SDL_Texture* prev = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, t);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
    SDL_RenderClear(renderer);
    // keep destination colour (white), take source alpha
    SDL_BlendMode keepWhite = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD,
                                                         SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ZERO, SDL_BLENDOPERATION_ADD);
    SDL_SetTextureBlendMode(pages[i], keepWhite);
    SDL_RenderTexture(renderer, pages[i], nullptr, nullptr);
    SDL_SetTextureBlendMode(pages[i], SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, prev);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    silhouettes[i] = t;
    return t;
}

}  // namespace flash
