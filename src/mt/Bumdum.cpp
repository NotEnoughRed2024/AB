#include "Bumdum.h"

#include "Timer.h"

namespace mt {

// ------------------------------------------------------------------ Col / Filt

void Col::setTransform(flash::DisplayObject* mc, double ra, double ga, double ba, double aa, double rb, double gb,
                       double bb, double ab) {
    if (!mc) return;
    mc->color = flash::ColorTransform(ra / 100.0, ga / 100.0, ba / 100.0, 1, rb, gb, bb, ab);
    mc->alpha = aa;
}

void Col::setPercentColor(flash::DisplayObject* mc, double prc, int col, double inc, double alpha) {
    RGB c0 = colToObj(col);
    double c = prc / 100.0;
    double cprc = hx::Int(100 - prc);
    setTransform(mc, cprc, cprc, cprc, alpha, hx::Int(c * c0.r + inc), hx::Int(c * c0.g + inc), hx::Int(c * c0.b + inc),
                 0);
}

void Col::setColor(flash::DisplayObject* mc, int col, double dec) {
    int r = (col >> 16) & 0xFF, g = (col >> 8) & 0xFF, b = col & 0xFF;
    setTransform(mc, 100, 100, 100, 100, hx::Int(r + dec), hx::Int(g + dec), hx::Int(b + dec), 0);
}

Col::RGB Col::getRainbow(double c) {
    if (hx::isNull(c)) c = hx::random();
    const int max = 3;
    double a[3];
    double part = (1.0 / max * 2);
    for (int i = 0; i < max; i++) {
        double med = part + i * 2 * part;
        double dif = Num::hMod(med - c, 0.5);
        // a negative channel can't happen in a real colour; Std.int keeps it in the Flash original too
        a[i] = std::min(1.5 - std::fabs(dif) * 3, 1.0);
    }
    return {hx::Int(a[0] * 255), hx::Int(a[1] * 255), hx::Int(a[2] * 255)};
}

int Col::mergeCol(int col, int col2, double c) {
    RGB o = colToObj(col), o2 = colToObj(col2);
    return objToCol({hx::Int(o.r * c + o2.r * (1 - c)), hx::Int(o.g * c + o2.g * (1 - c)), hx::Int(o.b * c + o2.b * (1 - c))});
}

void Filt::glow(flash::DisplayObject* mc, double bl, double str, int col, bool inner) {
    if (!mc) return;
    mc->filters.push_back(flash::Filter::glow(bl, str, (unsigned)col & 0xFFFFFF, inner));
}

void Filt::blur(flash::DisplayObject* mc, double blx, double bly) {
    if (!mc) return;
    mc->filters.push_back(flash::Filter::blur(blx, bly));
}

// ------------------------------------------------------------------ Sprite

std::vector<Sprite*> Sprite::spriteList;
std::vector<Sprite*> Sprite::graveyard;

void Sprite::collectGarbage() {
    for (auto* s : graveyard) delete s;
    graveyard.clear();
}

Sprite::Sprite(flash::MC mc) : root(std::move(mc)) {
    if (!root) {
        // the original passes clips in; a missing clip would crash Flash code too, keep a dummy
        root = std::make_shared<flash::MovieClip>(nullptr, nullptr);
    }
    root->onObjKill = [this]() { kill(); };
    spriteList.push_back(this);
    x = root->x;
    y = root->y;
    if (root->x == 0 && root->y == 0) {
        root->x = -100;
        root->y = -100;
        x = 0;
        y = 0;
    }
    scale = 100;
}

void Sprite::setScale(double n) {
    scale = n;
    root->xscale = n;
    root->yscale = n;
}

void Sprite::update() { updatePos(); }

void Sprite::kill() {
    if (dead) return;
    dead = true;
    root->onObjKill = nullptr;
    root->removeMovieClip();
    hx::remove(spriteList, this);
    graveyard.push_back(this);
}

void Sprite::updatePos() {
    root->x = x;
    root->y = y;
}

double Sprite::getDist(double ox, double oy) const {
    double dx = ox - x, dy = oy - y;
    return std::sqrt(dx * dx + dy * dy);
}
double Sprite::getAng(double ox, double oy) const { return std::atan2(oy - y, ox - x); }
void Sprite::toward(double ox, double oy, double c, double lim) {
    x += Num::mm(-lim, (ox - x) * c, lim);
    y += Num::mm(-lim, (oy - y) * c, lim);
}

// ------------------------------------------------------------------ Phys

Phys::Phys(flash::MC mc) : Sprite(std::move(mc)) {
    vx = 0;
    vy = 0;
    fadeLimit = 10;
    alpha = 100;
}

void Phys::setAlpha(double n) {
    alpha = n;
    root->alpha = alpha;
}

void Phys::update() {
    double tm = Timer::tmod;
    if (!hx::isNull(sleep)) {
        sleep--;
        if (sleep < 0) {
            sleep = hx::NUL;
            root->visible = true;
            root->play();
        }
        return;
    }
    if (!hx::isNull(weight)) vy += weight * tm;
    if (!hx::isNull(frict)) {
        double f = std::pow(frict, tm);
        vx *= f;
        vy *= f;
    }
    x += vx * tm;
    y += vy * tm;
    if (!hx::isNull(vr)) root->rotation += vr * tm;
    if (!hx::isNull(fr)) vr *= std::pow(fr, tm);
    if (!hx::isNull(vsc)) {
        root->xscale *= std::pow(vsc, tm);
        root->yscale = root->xscale;
    }
    if (!hx::isNull(timer)) {
        timer -= tm;
        if (timer < fadeLimit) {
            double c = timer / fadeLimit;
            switch (fadeType) {
                case -1:
                    break;
                case 0:
                    root->xscale = c * scale;
                    root->yscale = c * scale;
                    break;
                case 1:
                    root->visible = hx::Int(timer) % 4 > 1;
                    break;
                case 2:
                    root->play();
                    break;
                case 3:
                    root->yscale = c * scale;
                    break;
                case 4: {
                    double n = (1 - c) * 16;
                    Filt::blur(root.get(), n, 0);
                    root->alpha = c * alpha;
                    break;
                }
                case 5:
                    root->xscale = c * scale;
                    break;
                default:
                    root->alpha = c * alpha;
            }
            if (timer <= 0) {
                kill();
                return;
            }
        }
    }
    Sprite::update();
}

void Phys::towardSpeed(double ox, double oy, double c, double lim) {
    vx += Num::mm(-lim, (ox - x) * c, lim);
    vy += Num::mm(-lim, (oy - y) * c, lim);
}

void Part::update() {
    for (auto bh : bhl) {
        if (bh == HoriLine) root->xscale = vx * 100 * coef;
        else root->yscale = vy * 100 * coef;
    }
    if (bmp) bmp->drawMc(root.get());
    Phys::update();
}

// ------------------------------------------------------------------ Bmp / Plasma

Bmp::Bmp(flash::MC mc, int w, int h, bool transparent, unsigned color, double q) : root(std::move(mc)), pq(q) {
    bmp = std::make_shared<flash::BitmapData>(flash::Stage::get(), hx::Int(w * pq), hx::Int(h * pq), transparent, color);
    root->attachBitmap(bmp, 0);
    root->xscale = 100 / pq;
    root->yscale = 100 / pq;
}

void Bmp::kill() {
    if (bmp) bmp->dispose();
    root->removeMovieClip();
}

void Bmp::drawMc(flash::MovieClip* mc, double dx, double dy) {
    flash::Matrix m;
    m.scale((mc->xscale / 100) * pq, (mc->yscale / 100) * pq);
    m.rotate(mc->rotation * 0.0174);
    m.translate(mc->x * pq + dx, mc->y * pq + dy);
    flash::ColorTransform ct(1, 1, 1, 1, 0, 0, 0, -255 + mc->alpha * 2.55);
    bmp->draw(mc, m, &ct, mc->blendMode);
}

std::vector<Plasma*> Plasma::list;

Plasma::Plasma(flash::MC mc, int w, int h, double q) : Bmp(std::move(mc), w, h, true, 0, q) {
    list.push_back(this);
    fadeLimit = 10;
}

void Plasma::update() {
    if (ct) bmp->colorTransform(*ct);
    for (auto& f : filters) bmp->applyFilter(f);
    if (!hx::isNull(timer)) {
        timer -= Timer::tmod;
        if (timer < fadeLimit) {
            root->alpha = timer / fadeLimit * 100;
            if (timer <= 0) {
                kill();
                return;
            }
        }
    }
    if (root->removed) kill();
}

void Plasma::kill() {
    hx::remove(list, this);
    if (bmp) bmp->dispose();
    // owner (graveyard-style): plasmas are tiny; they are freed with the sprite graveyard
}

void Plasma::updateAll() {
    auto l = list;
    for (auto* p : l) p->update();
}

}  // namespace mt
