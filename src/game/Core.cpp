// Cs.hx, Element.hx, Bouncer.hx, Event.hx
#include <cmath>

#include "Elements.h"
#include "Shell.h"

// ------------------------------------------------------------------ Cs

bool Cs::DEMO = false;
const int Cs::DIR[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
int Cs::MX = 0, Cs::MY = 0;
int Cs::XMAX = 0, Cs::YMAX = 0;
double Cs::SIDE = 0;
double Cs::PREF_MOUSE = 0.2, Cs::PREF_GFX = 1;
std::vector<bool> Cs::PREF_BOOLS;
PlayerInfo* Cs::pi = nullptr;
int Cs::seed = 0;

void Cs::init() {
    XMAX = hx::Int(mcw / (double)BW);
    YMAX = hx::Int((mch - 30) / (double)BH);
    SIDE = (mcw - XMAX * BW) * 0.5;
    seed = hx::random(564351);
    auto* st = flash::Stage::get();
    auto& so = st->sharedObject("pref");
    if (!so.contains("mouse") || so["mouse"].is_null()) {
        so["mouse"] = 0.2;
        so["gfx"] = 1;
        st->flushSharedObject("pref");
    }
    if (!so.contains("bools") || so["bools"].is_null()) so["bools"] = {false, true, true, false};
    loadPref();
}

void Cs::loadPref() {
    auto& so = flash::Stage::get()->sharedObject("pref");
    PREF_MOUSE = so.value("mouse", 0.2);
    PREF_GFX = so.value("gfx", 1.0);
    PREF_BOOLS.clear();
    for (auto& b : so["bools"]) PREF_BOOLS.push_back(b.is_boolean() && b.get<bool>());
    while (PREF_BOOLS.size() < 4) PREF_BOOLS.push_back(false);
}

void Cs::log(const std::string& s) { SDL_Log("%s", s.c_str()); }

// ------------------------------------------------------------------ Element

Element::Element(MC mc) : Sprite(std::move(mc)) {
    ox = 0.5;
    oy = 0.5;
    px = 0;
    py = 0;
    vx = 0;
    vy = 0;
    frict = 1;
}

void Element::update() {
    if (flGhost) {
        updateGhost();
        return;
    }
    updateNormal();
}

void Element::updateNormal() {
    double parc = 1;
    vvx = vx * mt::Timer::tmod;
    vvy = vy * mt::Timer::tmod;
    while (parc > 0) {
        double cx, cy;
        if (vvx > 0) cx = (Cs::BW - ox) / vvx;
        else if (vvx < 0) cx = ox / vvx;
        else cx = 999999;
        if (vvy > 0) cy = (Cs::BH - oy) / vvy;
        else if (vvy < 0) cy = oy / vvy;
        else cy = 999999;
        double c;
        int sx = 0, sy = 0;
        bool hasSx = false, hasSy = false;
        if (std::fabs(cx) < std::fabs(cy)) {
            c = std::fabs(cx);
            sx = hx::Int(cx / c);
            if (c == 0) sx = -1;
            hasSx = true;
        } else {
            c = std::fabs(cy);
            sy = hx::Int(cy / c);
            if (c == 0) sy = -1;
            hasSy = true;
        }
        bool flCheck = true;
        if (c > parc) {
            c = parc;
            flCheck = false;
        }
        ox += vvx * c;
        oy += vvy * c;
        parc -= c;
        if (flCheck) {
            if (hasSx) {
                if (Game::me->isFree(px + sx, py)) {
                    px += sx;
                    ox -= sx * Cs::BW;
                    onEnterSquare(sx, 0);
                } else {
                    onBounce(px + sx, py);
                }
            }
            if (hasSy) {
                if (Game::me->isFree(px, py + sy)) {
                    py += sy;
                    oy -= sy * Cs::BH;
                    onEnterSquare(0, sy);
                } else {
                    onBounce(px, py + sy);
                }
            }
        }
    }
    x = Cs::getX(px) + ox;
    y = Cs::getY(py) + oy;
    Sprite::update();
}

void Element::setGhost(bool fl) {
    flGhost = fl;
    if (flGhost) {
        root->alpha = 25;
    } else {
        moveTo(x, y);
        root->alpha = 100;
    }
}

void Element::updateGhost() {
    x += vx * mt::Timer::tmod;
    y += vy * mt::Timer::tmod;
    if (x < Cs::SIDE || x > Cs::mcw - Cs::SIDE) {
        x = mt::Num::mm(Cs::SIDE, x, Cs::mcw - Cs::SIDE);
        vx *= -1;
    }
    if (y < 0) {
        y = 0;
        vy *= -1;
    }
    if (vy > 0) setGhost(false);
    root->x = x;
    root->y = y;
}

void Element::updatePos() {
    x = Cs::getX(px) + ox;
    y = Cs::getY(py) + oy;
    Sprite::updatePos();
}

void Element::moveTo(double nx, double ny) {
    px = hx::Int((nx - Cs::SIDE) / Cs::BW);
    py = hx::Int(ny / Cs::BH);
    ox = nx - Cs::getX(px);
    oy = ny - Cs::getY(py);
}

void Element::onBounce(int bpx, int bpy) {
    if (this->px != bpx) {
        vvx *= -frict;
        vx *= -frict;
    } else {
        vvy *= -frict;
        vy *= -frict;
    }
}

// ------------------------------------------------------------------ Bouncer

Bouncer::Bouncer(Phys* sprite) : sp(sprite) {
    frict = 1;
    setPos(sp->x, sp->y);
}

void Bouncer::update() {
    double parc = 1;
    double vvx = sp->vx * mt::Timer::tmod;
    double vvy = sp->vy * mt::Timer::tmod;
    while (parc > 0) {
        double cx, cy;
        if (vvx > 0) cx = (Cs::BW - ox) / vvx;
        else if (vvx < 0) cx = ox / vvx;
        else cx = 1;
        if (vvy > 0) cy = (Cs::BH - oy) / vvy;
        else if (vvy < 0) cy = oy / vvy;
        else cy = 1;
        double c;
        int sx = 0, sy = 0;
        bool hasSx = false, hasSy = false;
        if (std::fabs(cx) < std::fabs(cy)) {
            c = std::fabs(cx);
            sx = hx::Int(cx / c);
            if (c == 0) sx = -1;
            hasSx = true;
        } else {
            c = std::fabs(cy);
            sy = hx::Int(cy / c);
            if (c == 0) sy = -1;
            hasSy = true;
        }
        bool flCheck = true;
        if (c > parc) {
            c = parc;
            flCheck = false;
        }
        ox += vvx * c;
        oy += vvy * c;
        parc -= c;
        if (flCheck) {
            if (hasSx) {
                if (Game::me->isFree(px + sx, py)) {
                    px += sx;
                    ox -= sx * Cs::BW;
                } else {
                    onBounce(px + sx, py);
                    vvx *= -frict;
                    sp->vx *= -frict;
                }
            }
            if (hasSy) {
                if (Game::me->isFree(px, py + sy)) {
                    py += sy;
                    oy -= sy * Cs::BH;
                } else {
                    onBounce(px, py + sy);
                    vvy *= -frict;
                    sp->vy *= -frict;
                }
            }
        }
    }
    sp->x = Cs::getX(px) + ox;
    sp->y = Cs::getY(py) + oy;
}

void Bouncer::setPos(double nx, double ny) {
    px = hx::Int((nx - Cs::SIDE) / Cs::BW);
    py = hx::Int(ny / Cs::BH);
    ox = nx - Cs::getX(px);
    oy = ny - Cs::getY(py);
}

// ------------------------------------------------------------------ Event

Event::Event() {
    Game::me->events.push_back(this);
    Game::me->ownedEvents.emplace_back(this);
}

void Event::kill() {
    dead = true;
    if (Game::me) hx::remove(Game::me->events, this);
}
