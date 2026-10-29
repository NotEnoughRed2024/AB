#include "Stage.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include "Filters.h"

namespace flash {

Stage* Stage::instance = nullptr;

Stage::Stage(SDL_Window* w, SDL_Renderer* r, int wd, int ht, double f)
    : window(w), renderer(r), width(wd), height(ht), fps(f) {
    instance = this;
    startTicks = SDL_GetTicks();
    root = std::make_shared<MovieClip>(nullptr, nullptr);
    root->name = "_root";
    root->constructed = true;
    // probe custom blend mode support (the software renderer has none)
    if (SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 1, 1)) {
        SDL_BlendMode m = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDFACTOR_ONE,
                                                     SDL_BLENDOPERATION_REV_SUBTRACT, SDL_BLENDFACTOR_ZERO,
                                                     SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
        customBlend = SDL_SetTextureBlendMode(t, m);
        SDL_DestroyTexture(t);
    }
    if (!customBlend)
        SDL_Log("Renderer '%s' has no custom blend modes: Flash colour effects will be approximated",
                SDL_GetRendererName(r));
}

Stage::~Stage() {
    root.reset();
    TexturePool::get().clear();
    for (auto& [n, l] : libraries) l->unload();
    instance = nullptr;
}

Library* Stage::loadLibrary(const std::string& dir, const std::string& name) {
    auto lib = std::make_unique<Library>();
    if (!lib->load(renderer, dir, name)) return nullptr;
    Library* p = lib.get();
    libraries[name] = std::move(lib);
    if (!mainLib) {
        mainLib = p;
        root->lib = p;
    }
    return p;
}

double Stage::timeMs() const { return (double)(SDL_GetTicks() - startTicks); }

// ------------------------------------------------------------------ input

static int flashKeyCode(SDL_Keycode k) {
    if (k >= SDLK_A && k <= SDLK_Z) return 'A' + (int)(k - SDLK_A);
    if (k >= SDLK_0 && k <= SDLK_9) return '0' + (int)(k - SDLK_0);
    switch (k) {
        case SDLK_RETURN: case SDLK_KP_ENTER: return Key::ENTER;
        case SDLK_ESCAPE: return Key::ESCAPE;
        case SDLK_SPACE: return Key::SPACE;
        case SDLK_LEFT: return Key::LEFT;
        case SDLK_RIGHT: return Key::RIGHT;
        case SDLK_UP: return Key::UP;
        case SDLK_DOWN: return Key::DOWN;
        case SDLK_LSHIFT: case SDLK_RSHIFT: return Key::SHIFT;
        case SDLK_LCTRL: case SDLK_RCTRL: return Key::CONTROL;
        case SDLK_BACKSPACE: return Key::BACKSPACE;
        case SDLK_TAB: return Key::TAB;
        default: return -1;
    }
}

void Stage::handleEvent(const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_MOUSE_MOTION: {
            float fx = e.motion.x, fy = e.motion.y;
            SDL_RenderCoordinatesFromWindow(renderer, fx, fy, &fx, &fy);
            mouseX = (fx - viewX) / viewScale;
            mouseY = (fy - viewY) / viewScale;
            pointerInGame = insideGame(fx, fy);
            // in the browser the Flash plugin stopped seeing the pointer once it left (unless a button
            // was held): pin it to the edge of the game so reading the page doesn't steer the pad
            if (gameRect.w > 0 && !pointerInGame && !gameOwnsButton) {
                mouseX = std::max(0.0, std::min(mouseX, width - 0.01));
                mouseY = std::max(0.0, std::min(mouseY, height - 0.01));
            }
            applyCursor();
            updatePointer();
            auto ls = mouseListeners;
            for (auto& l : ls)
                if (l->onMouseMove) l->onMouseMove();
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (e.button.button != SDL_BUTTON_LEFT) break;
            float fx = e.button.x, fy = e.button.y;
            SDL_RenderCoordinatesFromWindow(renderer, fx, fy, &fx, &fy);
            if (!insideGame(fx, fy)) break;
            gameOwnsButton = true;
            mouseDown = true;
            {
                auto h = hover.lock();
                pressed = h;
                if (h && h->onPress) h->onPress();
                auto ls = mouseListeners;
                for (auto& l : ls)
                    if (l->onMouseDown) l->onMouseDown();
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (e.button.button != SDL_BUTTON_LEFT || !gameOwnsButton) break;
            gameOwnsButton = false;
            mouseDown = false;
            {
                auto p = pressed.lock();
                auto h = hover.lock();
                pressed.reset();
                if (p) {
                    if (p == h) {
                        if (p->onRelease) p->onRelease();
                    } else if (p->onReleaseOutside) {
                        p->onReleaseOutside();
                    }
                }
                auto ls = mouseListeners;
                for (auto& l : ls)
                    if (l->onMouseUp) l->onMouseUp();
            }
            break;
        case SDL_EVENT_KEY_DOWN: {
            int k = flashKeyCode(e.key.key);
            if (k < 0) break;
            bool repeat = e.key.repeat;
            keys[k] = true;
            lastKey = k;
            if (!repeat || true) {  // Flash fires onKeyDown on auto-repeat too
                auto ls = keyListeners;
                for (auto& l : ls)
                    if (l->onKeyDown) l->onKeyDown();
            }
            break;
        }
        case SDL_EVENT_KEY_UP: {
            int k = flashKeyCode(e.key.key);
            if (k < 0) break;
            keys[k] = false;
            lastKey = k;
            auto ls = keyListeners;
            for (auto& l : ls)
                if (l->onKeyUp) l->onKeyUp();
            break;
        }
        default:
            break;
    }
}

MC Stage::findInteractive(MovieClip* mc, double gx, double gy) {
    for (auto it = mc->children.rbegin(); it != mc->children.rend(); ++it) {
        auto& c = it->second;
        if (!c->visible || c->kind != DisplayObject::KClip) continue;
        auto* cm = static_cast<MovieClip*>(c.get());
        bool interactive = cm->enabled && (cm->onPress || cm->onRelease || cm->onRollOver || cm->onRollOut);
        if (interactive) {
            bool hit = cm->hitArea ? cm->hitArea->hitTest(gx, gy, true) : cm->hitTest(gx, gy, true);
            if (hit) return std::static_pointer_cast<MovieClip>(c);
        } else if (auto r = findInteractive(cm, gx, gy)) {
            return r;
        }
    }
    return nullptr;
}

void Stage::updatePointer() {
    MC now = findInteractive(root.get(), mouseX, mouseY);
    MC was = hover.lock();
    if (now == was) return;
    auto p = pressed.lock();
    if (was) {
        if (p == was && was->onDragOut) was->onDragOut();
        else if (was->onRollOut) was->onRollOut();
    }
    hover = now;
    if (now) {
        if (p == now && now->onDragOver) now->onDragOver();
        else if (now->onRollOver) now->onRollOver();
    }
    SDL_SetCursor(SDL_GetDefaultCursor());
}

// ------------------------------------------------------------------ frame

void Stage::queueFrameActions(const MC& mc) { actionQueue.push_back(mc); }

void Stage::runActionQueue() {
    for (int guard = 0; guard < 8 && !actionQueue.empty(); guard++) {
        auto q = std::move(actionQueue);
        actionQueue.clear();
        for (auto& w : q)
            if (auto mc = w.lock()) mc->runFrameActions();
    }
}

void Stage::collectEnterFrame(MovieClip* mc, std::vector<MC>& out) {
    for (auto& [d, c] : mc->children) {
        if (c->kind != DisplayObject::KClip) continue;
        auto* cm = static_cast<MovieClip*>(c.get());
        if (cm->onEnterFrame) out.push_back(std::static_pointer_cast<MovieClip>(c));
        collectEnterFrame(cm, out);
    }
}

void Stage::tick() {
    root->advance();
    runActionQueue();
    std::vector<MC> handlers;
    if (root->onEnterFrame) handlers.push_back(root);
    collectEnterFrame(root.get(), handlers);
    for (auto& h : handlers)
        if (!h->removed && h->onEnterFrame) {
            auto f = h->onEnterFrame;
            f();
        }
    runActionQueue();
    updatePointer();
}

// ------------------------------------------------------------------ rendering

SDL_BlendMode Stage::sdlBlend(Blend b, bool pm) const {
    switch (b) {
        case Blend::Add:
            return pm ? SDL_BLENDMODE_ADD_PREMULTIPLIED : SDL_BLENDMODE_ADD;
        case Blend::Subtract:
            return SDL_ComposeCustomBlendMode(pm ? SDL_BLENDFACTOR_ONE : SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDFACTOR_ONE,
                                              SDL_BLENDOPERATION_REV_SUBTRACT, SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE,
                                              SDL_BLENDOPERATION_ADD);
        case Blend::Multiply:
            return SDL_BLENDMODE_MUL;
        case Blend::Screen:
            return SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR, SDL_BLENDFACTOR_ONE,
                                              SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE,
                                              SDL_BLENDOPERATION_ADD);
        case Blend::Lighten:
            return SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_MAXIMUM,
                                              SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_MAXIMUM);
        case Blend::Darken:
            // GPU MIN ignores the blend factors, so transparent texels (black) would darken everything they
            // cover; multiply darkens the same way while honouring alpha
            return SDL_BLENDMODE_MUL;
        case Blend::Erase:
            return SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                              SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO,
                                              SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
        case Blend::Alpha:
            return SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
                                              SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
        default:
            return pm ? SDL_BLENDMODE_BLEND_PREMULTIPLIED : SDL_BLENDMODE_BLEND;
    }
}

static float clamp01(double v) { return (float)std::min(1.0, std::max(0.0, v)); }

void Stage::drawTexture(SDL_Texture* t, double w, double h, const Matrix& m, const ColorTransform& ct, Blend blend,
                        bool pm, const SDL_FRect* src) {
    // draws the rectangle (0,0)-(w,h) in local space through m; w/h are local units
    float tw, th;
    SDL_GetTextureSize(t, &tw, &th);
    SDL_FRect s = src ? *src : SDL_FRect{0, 0, tw, th};
    float u0 = s.x / tw, v0 = s.y / th, u1 = (s.x + s.w) / tw, v1 = (s.y + s.h) / th;
    double am = clamp01(ct.am);
    if (am <= 0) return;
    SDL_FColor col;
    if (pm) col = {clamp01(ct.rm) * (float)am, clamp01(ct.gm) * (float)am, clamp01(ct.bm) * (float)am, (float)am};
    else col = {clamp01(ct.rm), clamp01(ct.gm), clamp01(ct.bm), (float)am};
    Point p0 = m.apply(0, 0), p1 = m.apply(w, 0), p2 = m.apply(w, h), p3 = m.apply(0, h);
    SDL_Vertex v[4] = {
        {{(float)p0.x, (float)p0.y}, col, {u0, v0}},
        {{(float)p1.x, (float)p1.y}, col, {u1, v0}},
        {{(float)p2.x, (float)p2.y}, col, {u1, v1}},
        {{(float)p3.x, (float)p3.y}, col, {u0, v1}},
    };
    static const int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_SetTextureBlendMode(t, sdlBlend(blend, pm));
    SDL_RenderGeometry(renderer, t, v, 4, idx, 6);
}

void Stage::drawImage(Library* lib, int image, const Matrix& m, const ColorTransform& ct0, Blend blend) {
    const Image& im = lib->image(image);
    if (im.w <= 1 && im.h <= 1) return;
    double s = lib->scale;
    // local rect in logical units: origin at (-ox/s, -oy/s)
    Matrix lm = m * Matrix::make(1.0 / s, 0, 0, 1.0 / s, -im.ox / s, -im.oy / s);
    SDL_FRect src{(float)im.x, (float)im.y, (float)im.w, (float)im.h};
    ColorTransform ct = ct0;
    if (!customBlend) {
        // no silhouette/subtract passes available: fold offsets into the multiplier, exact for white art
        // (which is what the game tints), approximate otherwise
        auto fold = [](double& mult, double& add) {
            if (add < 0) mult = std::max(0.0, mult * (1 + add / 255.0));
            else if (add > 0) mult = mult + add / 255.0;
            add = 0;
        };
        fold(ct.rm, ct.ra);
        fold(ct.gm, ct.ga);
        fold(ct.bm, ct.ba);
        if (blend == Blend::Subtract || blend == Blend::Screen || blend == Blend::Lighten) blend = Blend::Add;
        if (blend == Blend::Darken || blend == Blend::Erase || blend == Blend::Alpha) blend = Blend::Normal;
    }
    drawTexture(lib->page(im.page), im.w, im.h, lm, ct, blend, false, &src);
    if (customBlend && ct.hasAdd() && ct.am > 0) {
        SDL_Texture* sil = lib->silhouettePage(im.page);
        double r = ct.ra, g = ct.ga, b = ct.ba;
        double pr = std::max(0.0, r), pg = std::max(0.0, g), pb = std::max(0.0, b);
        if (pr + pg + pb > 0) {
            ColorTransform add(pr / 255.0, pg / 255.0, pb / 255.0, ct.am);
            drawTexture(sil, im.w, im.h, lm, add, blend == Blend::Normal ? Blend::Add : blend, false, &src);
        }
        double nr = std::max(0.0, -r), ng = std::max(0.0, -g), nb = std::max(0.0, -b);
        if (nr + ng + nb > 0) {
            ColorTransform sub(nr / 255.0, ng / 255.0, nb / 255.0, ct.am);
            drawTexture(sil, im.w, im.h, lm, sub, Blend::Subtract, false, &src);
        }
    }
}

void Stage::renderObject(DisplayObject* o, const Matrix& m, const ColorTransform& ct, Blend blend, bool pmTarget) {
    if (!o->visible || o->removed) return;
    if (!o->filters.empty()) {
        renderFiltered(o, m, ct, blend, pmTarget);
        return;
    }
    Matrix lm = m * o->matrix();
    ColorTransform lct = o->colorTransform().concat(ct);
    if (lct.am <= 0 && lct.aa <= 0) return;
    Blend b = o->blendMode != Blend::Normal ? o->blendMode : blend;
    switch (o->kind) {
        case DisplayObject::KShape: {
            auto* s = static_cast<Shape*>(o);
            drawImage(s->lib, s->image, lm, lct, b);
            break;
        }
        case DisplayObject::KBitmap: {
            auto* bh = static_cast<BitmapHolder*>(o);
            BitmapData* bd = bh->bmp.get();
            if (!bd || !bd->texture) break;
            Matrix bm = lm * Matrix::make(1.0 / bd->res, 0, 0, 1.0 / bd->res, 0, 0);
            drawTexture(bd->texture, bd->texW(), bd->texH(), bm, lct, b, bd->transparent);
            break;
        }
        case DisplayObject::KText:
            renderText(static_cast<TextField*>(o), lm, lct, b);
            break;
        case DisplayObject::KClip: {
            auto* mc = static_cast<MovieClip*>(o);
            if (!mc->strokes.empty() || mc->pen.pts.size() > 1) {
                drawStrokes(mc->strokes, lm, lct, b);
                if (mc->penSet && mc->pen.pts.size() > 1) drawStrokes({mc->pen}, lm, lct, b);
            }
            for (auto it = mc->children.begin(); it != mc->children.end();) {
                DisplayObject* c = it->second.get();
                if (c->clipDepth > 0) {
                    decltype(it) resume;
                    renderMasked(mc, it, lm, lct, b, pmTarget, resume);
                    it = resume;
                    continue;
                }
                renderObject(c, lm, lct, b, pmTarget);
                ++it;
            }
            break;
        }
    }
}

// Renders `o` (with its transform m*o.matrix) into an offscreen premultiplied texture covering `area` (target px).
static SDL_Texture* renderToTexture(Stage* st, int w, int h, const std::function<void()>& draw) {
    SDL_Texture* t = TexturePool::get().acquire(st->renderer, w, h, SDL_TEXTUREACCESS_TARGET);
    SDL_Texture* prevT = st->currentTarget;
    int pw = st->targetW, ph = st->targetH;
    SDL_SetRenderTarget(st->renderer, t);
    st->currentTarget = t;
    st->targetW = w;
    st->targetH = h;
    SDL_SetRenderDrawColor(st->renderer, 0, 0, 0, 0);
    SDL_RenderClear(st->renderer);
    draw();
    SDL_SetRenderTarget(st->renderer, prevT);
    st->currentTarget = prevT;
    st->targetW = pw;
    st->targetH = ph;
    return t;
}

static bool areaFor(Stage* st, const Rect& r, double pad, int& x, int& y, int& w, int& h) {
    if (r.empty) return false;
    double x0 = std::floor(r.xMin - pad), y0 = std::floor(r.yMin - pad);
    double x1 = std::ceil(r.xMax + pad), y1 = std::ceil(r.yMax + pad);
    // keep offscreen buffers bounded to the visible target (+margin for blur)
    double lim = pad + 8;
    x0 = std::max(x0, -lim);
    y0 = std::max(y0, -lim);
    x1 = std::min(x1, st->targetW + lim);
    y1 = std::min(y1, st->targetH + lim);
    if (x1 <= x0 || y1 <= y0) return false;
    x = (int)x0;
    y = (int)y0;
    w = (int)(x1 - x0);
    h = (int)(y1 - y0);
    return w > 0 && h > 0 && w < 8192 && h < 8192;
}

void Stage::renderFiltered(DisplayObject* o, const Matrix& m, const ColorTransform& ct, Blend blend, bool pmTarget) {
    Matrix lm = m * o->matrix();
    ColorTransform lct = o->colorTransform().concat(ct);
    if (lct.am <= 0) return;
    double scaleOut = std::sqrt(std::fabs(m.a * m.d - m.b * m.c));  // parent->pixels scale
    if (scaleOut <= 0) return;
    double pad = 2;
    for (auto& f : o->filters) pad = std::max(pad, std::max(f.blurX, f.blurY) * scaleOut + f.distance * scaleOut + 2);
    Rect b = lm.transformRect(o->localBounds());
    int ax, ay, aw, ah;
    if (!areaFor(this, b, pad, ax, ay, aw, ah)) return;

    auto filters = o->filters;
    Blend ob = o->blendMode != Blend::Normal ? o->blendMode : blend;
    ColorTransform inner = lct;
    double finalAlpha = inner.am;
    inner.am = 1;
    SDL_Texture* t = renderToTexture(this, aw, ah, [&]() {
        auto saved = o->filters;
        o->filters.clear();
        Blend savedB = o->blendMode;
        o->blendMode = Blend::Normal;
        double savedAlpha = o->alpha;
        o->alpha = 100;
        Matrix shift = Matrix::make(1, 0, 0, 1, -ax, -ay) * m;
        ColorTransform pct = ct;
        pct.am = 1;
        renderObject(o, shift, pct, Blend::Normal, true);
        o->filters = saved;
        o->blendMode = savedB;
        o->alpha = savedAlpha;
    });
    // CPU filter pass
    SDL_Texture* prevT = currentTarget;
    SDL_SetRenderTarget(renderer, t);
    SDL_Surface* s = SDL_RenderReadPixels(renderer, nullptr);
    SDL_SetRenderTarget(renderer, prevT);
    if (s) {
        SDL_Surface* cs = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(s);
        if (cs) {
            PixelBuffer pb;
            pb.w = cs->w;
            pb.h = cs->h;
            pb.px.resize((size_t)pb.w * pb.h);
            for (int yy = 0; yy < pb.h; yy++)
                SDL_memcpy(&pb.px[(size_t)yy * pb.w], (Uint8*)cs->pixels + yy * cs->pitch, (size_t)pb.w * 4);
            SDL_DestroySurface(cs);
            for (auto& f : filters) applyFilterCPU(pb, f, scaleOut);
            SDL_Texture* up = TexturePool::get().acquire(renderer, aw, ah, SDL_TEXTUREACCESS_STREAMING);
            SDL_UpdateTexture(up, nullptr, pb.px.data(), pb.w * 4);
            ColorTransform fin(1, 1, 1, finalAlpha);
            SDL_FRect sr{0, 0, (float)aw, (float)ah};
            drawTexture(up, aw, ah, Matrix::make(1, 0, 0, 1, ax, ay), fin, ob, true, &sr);
        }
    }
}

void Stage::renderMasked(MovieClip* mc, std::map<int, std::shared_ptr<DisplayObject>>::iterator maskIt,
                         const Matrix& m, const ColorTransform& ct, Blend blend, bool pmTarget,
                         std::map<int, std::shared_ptr<DisplayObject>>::iterator& resume) {
    DisplayObject* mask = maskIt->second.get();
    int limit = mask->clipDepth - 16384;
    auto first = std::next(maskIt);
    auto last = first;
    while (last != mc->children.end() && last->first <= limit) ++last;
    resume = last;
    if (first == last) return;
    Rect b = (m * mask->matrix()).transformRect(mask->localBounds());
    int ax, ay, aw, ah;
    if (!areaFor(this, b, 1, ax, ay, aw, ah)) return;
    Matrix shift = Matrix::make(1, 0, 0, 1, -ax, -ay) * m;
    SDL_Texture* content = renderToTexture(this, aw, ah, [&]() {
        for (auto it = first; it != last; ++it) renderObject(it->second.get(), shift, ct, blend, true);
    });
    SDL_Texture* mtex = renderToTexture(this, aw, ah, [&]() {
        bool v = mask->visible;
        mask->visible = true;
        renderObject(mask, shift, ColorTransform(), Blend::Normal, true);
        mask->visible = v;
    });
    // content *= mask alpha
    SDL_Texture* prevT = currentTarget;
    SDL_SetRenderTarget(renderer, content);
    SDL_BlendMode mul = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_ALPHA,
                                                   SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO,
                                                   SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
    SDL_SetTextureBlendMode(mtex, mul);
    SDL_RenderTexture(renderer, mtex, nullptr, nullptr);
    SDL_SetRenderTarget(renderer, prevT);
    SDL_FRect sr{0, 0, (float)aw, (float)ah};
    drawTexture(content, aw, ah, Matrix::make(1, 0, 0, 1, ax, ay), ColorTransform(), Blend::Normal, true, &sr);
}

void Stage::drawStrokes(const std::vector<MovieClip::Stroke>& strokes, const Matrix& m, const ColorTransform& ct,
                        Blend blend) {
    double sc = std::sqrt(std::fabs(m.a * m.d - m.b * m.c));
    for (const auto& st : strokes) {
        if (st.pts.size() < 2) continue;
        double r = ((st.color >> 16) & 255), g = ((st.color >> 8) & 255), b = (st.color & 255), a = st.alpha * 255;
        ct.applyTo(r, g, b, a);
        SDL_FColor col{clamp01(r / 255), clamp01(g / 255), clamp01(b / 255), clamp01(a / 255)};
        double w = std::max(1.0, st.thickness * sc) * 0.5;  // half width in target pixels (hairline = 1px)
        std::vector<SDL_Vertex> v;
        std::vector<int> idx;
        for (size_t i = 1; i < st.pts.size(); i++) {
            Point p0 = m.apply(st.pts[i - 1].x, st.pts[i - 1].y), p1 = m.apply(st.pts[i].x, st.pts[i].y);
            double dx = p1.x - p0.x, dy = p1.y - p0.y, len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-6) continue;
            double nx = -dy / len * w, ny = dx / len * w;
            // extend ends slightly so consecutive segments overlap at joints
            double ex = dx / len * w * 0.5, ey = dy / len * w * 0.5;
            int base = (int)v.size();
            v.push_back({{(float)(p0.x + nx - ex), (float)(p0.y + ny - ey)}, col, {0, 0}});
            v.push_back({{(float)(p1.x + nx + ex), (float)(p1.y + ny + ey)}, col, {0, 0}});
            v.push_back({{(float)(p1.x - nx + ex), (float)(p1.y - ny + ey)}, col, {0, 0}});
            v.push_back({{(float)(p0.x - nx - ex), (float)(p0.y - ny - ey)}, col, {0, 0}});
            int q[6] = {base, base + 1, base + 2, base, base + 2, base + 3};
            idx.insert(idx.end(), q, q + 6);
        }
        if (v.empty()) continue;
        SDL_SetRenderDrawBlendMode(renderer, sdlBlend(blend, false));
        SDL_RenderGeometry(renderer, nullptr, v.data(), (int)v.size(), idx.data(), (int)idx.size());
    }
}

void Stage::updateView() {
    int ow, oh;
    SDL_GetCurrentRenderOutputSize(renderer, &ow, &oh);
    double ax = 0, ay = 0, aw = ow, ah = oh;
    if (gameRect.w > 0 && gameRect.h > 0) {
        ax = gameRect.x;
        ay = gameRect.y;
        aw = gameRect.w;
        ah = gameRect.h;
    }
    viewScale = std::min(aw / width, ah / height);
    viewX = std::floor(ax + (aw - width * viewScale) / 2);
    viewY = std::floor(ay + (ah - height * viewScale) / 2);
    view = Matrix::make(viewScale, 0, 0, viewScale, viewX, viewY);
    targetW = ow;
    targetH = oh;
}

bool Stage::insideGame(float wx, float wy) const {
    double gw = width * viewScale, gh = height * viewScale;
    return wx >= viewX && wy >= viewY && wx < viewX + gw && wy < viewY + gh;
}

void Stage::applyCursor() {
    bool hide = mouseHidden && pointerInGame;
    if (hide == SDL_CursorVisible()) {
        if (hide) SDL_HideCursor();
        else SDL_ShowCursor();
    }
}

void Stage::render(bool clear) {
    updateView();
    currentTarget = nullptr;
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_Rect clip{(int)viewX, (int)viewY, (int)std::ceil(width * viewScale), (int)std::ceil(height * viewScale)};
    if (clear) {
        SDL_RenderClear(renderer);
    } else {
        SDL_FRect bg{(float)clip.x, (float)clip.y, (float)clip.w, (float)clip.h};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_RenderFillRect(renderer, &bg);
    }
    SDL_SetRenderClipRect(renderer, &clip);
    renderObject(root.get(), view, ColorTransform(), Blend::Normal, false);
    SDL_SetRenderClipRect(renderer, nullptr);
    TexturePool::get().endFrame();
}

// ------------------------------------------------------------------ SharedObject

static std::string soPath(const std::string& name) {
    char* base = SDL_GetPrefPath("MotionTwinFans", "Alphabounce");
    std::string p = base ? std::string(base) : std::string("./");
    SDL_free(base);
    return p + name + ".json";
}

nlohmann::json& Stage::sharedObject(const std::string& name) {
    auto it = shared.find(name);
    if (it != shared.end()) return it->second;
    nlohmann::json j = nlohmann::json::object();
    size_t sz = 0;
    void* raw = SDL_LoadFile(soPath(name).c_str(), &sz);
    if (raw) {
        try {
            j = nlohmann::json::parse((const char*)raw, (const char*)raw + sz);
        } catch (...) {
            SDL_Log("SharedObject %s is corrupt; starting fresh", name.c_str());
        }
        SDL_free(raw);
    }
    return shared[name] = j;
}

void Stage::flushSharedObject(const std::string& name) {
    auto it = shared.find(name);
    if (it == shared.end()) return;
    std::string s = it->second.dump();
    SDL_SaveFile(soPath(name).c_str(), s.data(), s.size());
}

}  // namespace flash
