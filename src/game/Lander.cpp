// lander/*.hx and navi/menu/{People,Furi,Traveler,ItemGiver,Info}.hx
#include "Lander.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Shell.h"
#include "flash/Filters.h"
#include "flash/Stage.h"

namespace Col = mt::Col;
namespace Filt = mt::Filt;
namespace Num = mt::Num;
using flash::Matrix;

namespace lander {

static const double NUL = hx::NUL;

// ================================================================== CPU bitmap helpers

static inline uint32_t px4(double a, double r, double g, double b) {
    auto c = [](double v) { return (uint32_t)std::lround(std::clamp(v, 0.0, 255.0)); };
    return (c(a) << 24) | (c(r) << 16) | (c(g) << 8) | c(b);
}

// premultiplied "normal" blend of s over d
static inline uint32_t over(uint32_t s, uint32_t d) {
    double sa = (s >> 24) / 255.0;
    if (sa >= 1) return s;
    if (sa <= 0) return d;
    double k = 1 - sa;
    return px4((s >> 24) + (d >> 24) * k, ((s >> 16) & 255) + ((d >> 16) & 255) * k,
               ((s >> 8) & 255) + ((d >> 8) & 255) * k, (s & 255) + (d & 255) * k);
}

static inline uint32_t erase(uint32_t s, uint32_t d) {
    double k = 1 - (s >> 24) / 255.0;
    if (k >= 1) return d;
    return px4((d >> 24) * k, ((d >> 16) & 255) * k, ((d >> 8) & 255) * k, (d & 255) * k);
}

// BitmapData.draw(clip, m, null, blend): rendered on the GPU at the library's resolution, read back and
// averaged down to one pixel per stage pixel, then blended into the CPU bitmap
void Game::drawClip(CpuBmp& dst, flash::DisplayObject* o, const Matrix& m, flash::Blend blend) {
    if (!o) return;
    flash::Rect b = m.transformRect(o->localBounds());
    if (b.empty) return;
    int x0 = std::max(0, (int)std::floor(b.xMin) - 1), y0 = std::max(0, (int)std::floor(b.yMin) - 1);
    int x1 = std::min(dst.w, (int)std::ceil(b.xMax) + 1), y1 = std::min(dst.h, (int)std::ceil(b.yMax) + 1);
    if (x1 <= x0 || y1 <= y0) return;
    int w = x1 - x0, h = y1 - y0;
    flash::BitmapData tmp(flash::Stage::get(), w, h, true, 0);
    tmp.draw(o, Matrix::make(1, 0, 0, 1, -x0, -y0) * m);
    std::vector<uint32_t> hi = tmp.readPixels();  // premultiplied, tmp.res pixels per stage pixel
    int tw = tmp.texW(), th = tmp.texH();
    int k = std::max(1, (int)std::lround(tmp.res));
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            double a = 0, r = 0, g = 0, bb = 0;
            int n = 0;
            for (int sy = 0; sy < k; sy++)
                for (int sx = 0; sx < k; sx++) {
                    int hx_ = x * k + sx, hy = y * k + sy;
                    if (hx_ >= tw || hy >= th) continue;
                    uint32_t p = hi[(size_t)hy * tw + hx_];
                    a += p >> 24;
                    r += (p >> 16) & 255;
                    g += (p >> 8) & 255;
                    bb += p & 255;
                    n++;
                }
            if (!n) continue;
            uint32_t s = px4(a / n, r / n, g / n, bb / n);
            if ((s >> 24) == 0) continue;
            uint32_t& d = dst.px[(size_t)(y0 + y) * dst.w + (x0 + x)];
            d = blend == flash::Blend::Erase ? erase(s, d) : over(s, d);
        }
}

void Game::drawBmp(CpuBmp& dst, const CpuBmp& src, int dx, int dy) {
    for (int y = 0; y < src.h; y++) {
        int ty = y + dy;
        if (ty < 0 || ty >= dst.h) continue;
        for (int x = 0; x < src.w; x++) {
            int tx = x + dx;
            if (tx < 0 || tx >= dst.w) continue;
            uint32_t s = src.px[(size_t)y * src.w + x];
            if (s >> 24) dst.px[(size_t)ty * dst.w + tx] = over(s, dst.px[(size_t)ty * dst.w + tx]);
        }
    }
}

// a display copy of a CPU bitmap (each pixel doubled to the texture resolution)
flash::BMP Game::show(const CpuBmp& b) {
    auto bmp = std::make_shared<flash::BitmapData>(flash::Stage::get(), std::max(1, b.w), std::max(1, b.h), true, 0);
    int tw = bmp->texW(), th = bmp->texH();
    std::vector<uint32_t> up((size_t)tw * th, 0);
    for (int y = 0; y < th; y++) {
        int sy = std::min(b.h - 1, (int)(y / bmp->res));
        for (int x = 0; x < tw; x++) {
            int sx = std::min(b.w - 1, (int)(x / bmp->res));
            if (sx >= 0 && sy >= 0) up[(size_t)y * tw + x] = b.px[(size_t)sy * b.w + sx];
        }
    }
    bmp->writePixels(up);
    shown.push_back(bmp);
    return bmp;
}

static void glowCpu(CpuBmp& b, double blur, double strength, unsigned color) {
    flash::PixelBuffer pb;
    pb.w = b.w;
    pb.h = b.h;
    pb.px = std::move(b.px);
    flash::applyFilterCPU(pb, flash::Filter::glow(blur, strength, color), 1.0);
    b.px = std::move(pb.px);
}

// ================================================================== Game

Game* Game::me = nullptr;

static Planete P(int type, double pop, double g, int hor, int gMax, int dMax, double hc, int m0, int m1,
                 std::vector<std::array<int, 3>> gri) {
    Planete p{type, pop, g, hor, gMax, dMax, hc, {m0, m1}, std::move(gri)};
    return p;
}

const std::vector<Planete> Game::PLANETES = {
    P(0, 1.6, 1.0, 80, 120, 400, 0.1, 50, 150, {{4, 4, 0xDB0202}, {4, 2, 0xFB0D0D}}),                     // 0 MOLTEAR
    P(1, 2.0, 0.8, 120, 120, 400, 0.25, 50, 20, {{12, 12, 0x58B858}, {4, 2, 0xCCFFAA}}),                 // 1 SOUPALINE
    P(0, 1.4, 0.7, 120, 120, 400, 0.05, 75, 0, {{10, 10, 0x8C4A2F}, {4, 2, 0x794028}}),                  // 2 LYCANS
    P(0, 1.3, 3.5, 120, 200, 440, 0.1, 100, 250, {{10, 10, 0xB05E0B}, {8, 8, 0xE3B602}, {4, 2, 0xFFCC00}}),  // 3 SAMOSA
    P(0, 1.3, 0.7, 120, 120, 150, 0.35, 10, 150, {{6, 6, 0xEBD994}, {4, 2, 0xFCE9A3}}),                  // 4 TIBOON
    P(0, 2.5, 2.5, 120, 100, 400, 0.10, 100, 0, {{8, 8, 0x4E1700}, {4, 2, 0x743D19}}),                   // 5 BALIXT
    P(0, NUL, 0.7, 120, 120, 400, 0.25, 50, 10, {{12, 12, 0x58B858}, {4, 2, 0xCCFFAA}}),                 // 6
    P(0, 1.2, 1.5, 120, 120, 400, 0.10, 75, 120, {{20, 20, 0xFFFFFF}, {4, 2, 0xAAFFFF}}),                // 7 SPIGNYSOS
    P(0, 1.5, 1.8, 120, 80, 300, 0.25, 150, 0, {{4, 4, 0x58B858}, {4, 2, 0xCCFFAA}}),                    // 8 POFIAK
    P(0, NUL, 0.5, 160, 0, 0, 0.15, 0, 0, {{12, 12, 0}, {4, 2, 0}}),                                     // 9 SENEGARDE
    P(0, 1.8, 2.5, 120, 120, 400, 0.1, 300, 0, {{4, 4, 0x8C1408}, {8, 4, 0xCF0D41}}),                    // 10 DOURIV
    P(0, 1.01, 0.7, 120, 120, 400, 0.25, 10, 0, {{10, 10, 0x888888}, {6, 4, 0xAAAAAA}}),                 // 11 GRIMORN
    P(0, 1.4, 1.0, 120, 120, 400, 0.25, 150, 10, {{12, 12, 0x787831}, {4, 2, 0x95954E}}),                // 12 D-TRITUS
    P(0, 1.01, 0.2, 160, 120, 400, 0.0, 100, 10, {{6, 6, 0x835830}, {4, 4, 0xA06D3D}}),                  // 13 ASTEROIDE
    P(0, 2.0, 1.0, 120, 120, 400, 0.1, 150, 50, {{6, 6, 0x34A0A0}, {4, 4, 0x4ABFBF}}),                   // 14 NALIKORS
    P(0, 1.8, 1.8, 120, 120, 400, 0.1, 100, 50, {{6, 6, 0x54104E}, {4, 4, 0xA33D88}}),                   // 15 HOLOVAN
    P(1, 1.5, 1.2, 120, 120, 400, 0.1, 150, 50, {{6, 6, 0x89B388}, {4, 4, 0xB0CAAC}}),                   // 16 KHORLAN
    P(1, 1.5, 1.5, 120, 120, 400, 0.1, 150, 50, {{6, 6, 0x915874}, {4, 4, 0xAE7591}}),                   // 17 CILORILE
    P(0, 1.01, 1.0, 120, 120, 400, 0.25, 20, 50, {{6, 6, 0x7C7869}, {4, 4, 0x989489}}),                  // 18 TARCITURNE
    P(0, 1.01, 1.0, 120, 120, 400, 0.25, 300, 50, {{6, 6, 0x657C77}, {2, 2, 0x92A5A1}}),                 // 19 CHAGARINA
    P(1, 1.5, 1.0, 120, 120, 300, 0.25, 150, 0, {{2, 2, 0x7E4686}, {2, 2, 0x9F57A8}}),                   // 20 VOLCER
    P(0, 1.25, 1.0, 120, 120, 400, 0.25, 20, 50, {{4, 4, 0x764A38}, {6, 4, 0xAC6D53}}),                  // 21 BALMANCH
    P(0, NUL, 0.5, 160, 0, 0, 0.15, 0, 0, {{12, 12, 0}, {4, 2, 0}}),                                     // 22 FOLKET
    P(2, NUL, 0.8, 120, 80, 400, 0.25, 0, 0, {{12, 12, 0x555555}, {4, 2, 0x888888}}),                    // 23 TERRE
};

bool Game::isAlive(int id) {
    if (id < 0 || id >= (int)PLANETES.size()) return false;
    double pop = PLANETES[id].pop;
    return !hx::isNull(pop) && pop >= 1.1;
}

bool Game::debugKit() { return flash::Stage::get() && flash::Stage::get()->debug.lander; }

Game::Game(MC mc, int col, bool flh) : Module(std::move(mc)), flHouseVisited(flh), col(col) {
    me = this;
    min = 0;
    missile = Cs::pi->missile;
    initInter();
}

Game::~Game() {
    if (me == this) me = nullptr;
}

void Game::initLevel(int x, int y, int zid, bool flMinerai, const std::string* lvl) {
    Module::initLevel(x, y, zid, flMinerai, lvl);
    seed = mt::OldRandom(x * 10000 + y);
    pl = &PLANETES[std::clamp(zid, 0, (int)PLANETES.size() - 1)];

    // BG
    bg = dm->attach("bgLander", DP_BG);
    bmpBg = getBmpBg(col);
    bg->attachBitmap(bmpBg, 0);

    // PLANS
    initPlans();

    if (pl->gMax > 0) {
        // GROUND
        mcGround = bdm->empty(DP_GROUND);
        Filt::glow(mcGround.get(), 2, 2, 0);
        gdm = std::make_unique<mt::DepthManager>(mcGround);
        bmpGround = CpuBmp(WIDTH, pl->gMax);
        bmpForeground = CpuBmp(WIDTH, pl->dMax);
        drawGround();

        // DECOR
        MC mcDecor = gdm->empty(DP_DECOR);
        mcDecor->setY(HEIGHT - pl->dMax);
        bmpDecor = CpuBmp(WIDTH, pl->dMax);
        drawDecor();

        // HOUSES
        int itemId = -1;
        Scenario scn = House::getScenario(level->zid, seed, itemId);
        if (scn != Scenario::None) {
            int rnd = seed.random(10000);
            ownedHouses.push_back(std::make_unique<House>(rnd, scn, itemId));
        }
        genColBmp();

        // FOREGROUND
        mcForeground = bdm->empty(DP_FOREGROUND);
        mcForeground->blendMode = flash::Blend::Layer;
        fdm = std::make_unique<mt::DepthManager>(mcForeground);
        drawBmp(bmpForeground, bmpGround, 0, pl->dMax - pl->gMax);
        MC fmc = fdm->empty(0);
        fmc->attachBitmap(show(bmpForeground), 0);
        fmc->setY(HEIGHT - pl->dMax);
        Filt::glow(fmc.get(), 1, 1, 0);

        // the house cavities
        for (auto* h : houses) h->dig();
        mcDecor->attachBitmap(show(bmpDecor), 0);

        // MINERALS
        if (flMinerai) initMinerals();
    }

    initPad();
    updatePlans();
    updateSprites();
}

void Game::initPad() {
    pad = new Pad();
    pad->x = WIDTH * 0.5;
    pad->y = Cs::mch * 0.5;
    if (const char* tx = SDL_getenv("AB_TEST_LANDER_X")) pad->x = std::atof(tx);  // testing
    if (const char* ty = SDL_getenv("AB_TEST_LANDER_Y")) pad->y = std::atof(ty);
    Pad* p = pad;
    focus = [p] { return flash::Point{p->x, p->y}; };
}

void Game::initMinerals() {
    int valueMax = hx::Int(std::pow(seed.rand(), 2) * pl->min[0]);
    while (valueMax > 0) {
        auto* m = new Mineral(seed);
        ownedMinerals.emplace_back(m);
        int value = hx::Int(Num::mm(1, seed.random(valueMax), 30));
        valueMax -= value;
        m->setValue(value);
        m->root->setX(seed.random(WIDTH));
        m->dropToSurface();
        for (auto* h : houses)
            if (std::abs(h->x - m->root->x) < 90) m->kill();
    }
}

void Game::initPlay() {
    Module::initPlay();
    step = Play;
    initKeyListener();
}

void Game::update() {
    if (!flView) return;
    Module::update();
    if (!hx::isNull(pauseCoef)) return;
    switch (step) {
        case Play:
            updatePlans();
            if (hero) hero->update();
            if (pad && !pad->dead && pad->y < -(pad->ray + 10) && pad->vy < 0) initEnding(true);
            break;
        case End: updateEnding(); break;
        case Outro: updateOutro(); break;
    }
    if (!me) return;
    updateSprites();
    flClick = false;
}

// ------------------------------------------------------------------ PLANS

void Game::initPlans() {
    plans.clear();
    // HORIZON
    {
        Plan p;
        p.mc = dm->attach("horLander", DP_PLAN);
        p.mc->gotoAndStop(level->zid + 1);
        p.c = pl->hc;
        p.flHor = true;
        plans.push_back(p);
    }
    // PLANS
    MC probe = dm->attach("landerPlan", DP_PLAN);
    probe->gotoAndStop(level->zid + 1);
    int max = probe->smc() ? probe->smc()->totalFrames() : 1;
    probe->removeMovieClip();
    for (int i = 0; i < max; i++) {
        double cc = std::pow((double)(i + 1) / max, 2);
        double c = pl->hc + cc * (1 - pl->hc);
        Plan p;
        if (i == max - 1) {
            p.mc = dm->empty(DP_PLAN);
            p.flHor = false;
            bdm = std::make_unique<mt::DepthManager>(p.mc);
            base = p.mc;
        } else {
            p.mc = dm->attach("landerPlan", DP_PLAN);
            double xmax = WIDTH * c + Cs::mcw * (1 - c);
            p.bx = c > 0 ? seed.rand() * (xmax / c) : 0;
            double dy = 0;
            if (level->zid == ZoneInfo::ASTEROBELT) dy = 400;
            p.flHor = true;
            p.dy = -seed.rand() * dy;
            p.mc->gotoAndStop(level->zid + 1);
            if (MC s = p.mc->smc()) {
                s->gotoAndStop(i + 1);
                if (MC s2 = s->smc()) {
                    s2->gotoAndStop(seed.random(s2->totalFrames()) + 1);
                    if (MC s3 = s2->smc()) s3->gotoAndStop(seed.random(s3->totalFrames()) + 1);
                }
            }
            p.mc->setXScale(c * 100);
            p.mc->setYScale(c * 100);
        }
        p.c = c;
        plans.push_back(p);
    }
    updatePlans();

    // AIR
    mcAir = dm->empty(DP_PLAN);
    mt::DepthManager adm(mcAir);
    int amax = hx::Int(Cs::PREF_GFX * 20);
    for (int i = 0; i < amax; i++) {
        MC mc = adm.attach("partAir", 0);
        mc->setX(hx::random() * Cs::mcw);
        mc->setY(hx::random() * Cs::mch);
        airList.push_back({mc, 1 + hx::random()});
    }
}

void Game::updatePlans() {
    if (!base || !focus) return;
    double obx = base->x, oby = base->y;
    flash::Point f = focus();
    double sx = Num::mm(Cs::mcw - WIDTH, Cs::mcw * 0.5 - f.x, 0);
    double sy = Num::mm(Cs::mch - HEIGHT, Cs::mch * 0.5 - f.y, 0);
    for (auto& p : plans) {
        if (p.flHor) {
            double dy = Cs::mch - pl->hor;
            double by = HEIGHT - (dy + GMIN);
            p.mc->setX((p.bx + sx) * p.c);
            p.mc->setY((sy + p.dy + by) * p.c + dy);
        } else {
            p.mc->setX(sx * p.c);
            p.mc->setY(sy * p.c);
        }
    }
    if (!mcAir) return;
    double dx = obx - base->x, dy = oby - base->y;
    mcAir->setAlpha((std::abs(dx) + std::abs(dy)) * 10);
    for (auto& a : airList) {
        a.mc->setX(Num::sMod(a.mc->x - dx * a.c, Cs::mcw));
        a.mc->setY(Num::sMod(a.mc->y - dy * a.c, Cs::mch));
    }
}

// ------------------------------------------------------------------ DECOR

void Game::drawGround() {
    const int W = WIDTH, H = pl->gMax;
    // SHAPE: a random skyline filled down to the bottom (the original drew it with the drawing API)
    std::vector<double> xs, ys;
    double gy = pl->gMax - GMIN;
    double x = 0, y = gy * 0.5;
    xs.push_back(0);
    ys.push_back(y);
    while (true) {
        x = std::min(x + 50 + seed.rand() * 200, (double)W);
        y = Num::mm(25, y + (seed.rand() * 2 - 1) * 40, gy);
        xs.push_back(x);
        ys.push_back(y);
        if (x == W) break;
    }
    std::vector<double> top(W);
    size_t seg = 0;
    for (int px = 0; px < W; px++) {
        double cx = px + 0.5;
        while (seg + 1 < xs.size() - 1 && xs[seg + 1] < cx) seg++;
        double x0 = xs[seg], x1 = xs[seg + 1];
        double t = x1 > x0 ? (cx - x0) / (x1 - x0) : 0;
        top[px] = ys[seg] + (ys[seg + 1] - ys[seg]) * t;
    }

    // TEXT: the planet's ground texture, a 100x100 tile
    const int side = 100;
    flash::BitmapData text(flash::Stage::get(), side, side, false, 0);
    MC mcText = dm->attach("groundText", 0);
    mcText->gotoAndStop(level->zid + 1);
    CpuBmp tile(side, side);
    for (auto& p : tile.px) p = 0xFF000000u;
    drawClip(tile, mcText.get(), Matrix());
    mcText->removeMovieClip();

    // APPLY: copyPixels(text, ..., shape) with the shape's coverage as alpha
    for (int py = 0; py < H; py++)
        for (int px = 0; px < W; px++) {
            double cov = std::clamp(py + 1 - top[px], 0.0, 1.0);
            if (cov <= 0) continue;
            uint32_t t = tile.px[(size_t)(py % side) * side + (px % side)];
            bmpGround.px[(size_t)py * W + px] =
                px4(255 * cov, ((t >> 16) & 255) * cov, ((t >> 8) & 255) * cov, (t & 255) * cov);
        }

    // SOL: the coloured crust
    for (auto& a : pl->gri) glowCpu(bmpGround, a[0], a[1], (unsigned)a[2]);
}

void Game::drawDecor() {
    MC mc = dm->attach("landerDecor", 0);
    mc->gotoAndStop(level->zid + 1);
    for (int i = 0; i < 20; i++) {
        int x = seed.random(WIDTH);
        int y = getGround(x);
        Pix p(x, y, 1);
        double a = p.getNormal(nullptr, 10);
        y -= HEIGHT - pl->dMax;
        Matrix m;
        m.rotate(a + 1.57);
        m.translate(x, y);
        if (MC s = mc->smc()) {
            s->gotoAndStop(seed.random(s->totalFrames()) + 1);
            if (MC s2 = s->smc()) s2->gotoAndStop(seed.random(s2->totalFrames()) + 1);
        }
        drawClip(bmpDecor, mc.get(), m);
    }
    mc->removeMovieClip();
    drawBmp(bmpDecor, bmpGround, 0, pl->dMax - pl->gMax);
}

void Game::genColBmp() {
    bmpCol = bmpDecor;
    drawBmp(bmpCol, bmpGround, 0, pl->dMax - pl->gMax);
    glowCpu(bmpCol, 8, 60, 0);
}

Hero* Game::newHero() {
    int skinId = 0;
    if (pl->type == 1 && !Cs::pi->gotItem(MissionInfo::COMBINAISON) && !debugKit()) skinId = 1;
    auto* h = new Hero(skinId);
    addHero(h);
    Hero* hp = h;
    focus = [hp] { return flash::Point{(double)hp->x, (double)hp->y}; };
    if (pl->type == 2) h->flControl = false;
    return h;
}

void Game::addHero(Hero* h) {
    heroes.emplace_back(h);
    hero = h;
}

void Game::retireHero(Hero* h) {
    if (hero == h) hero = nullptr;  // freed with the game: it may still be running its update
}

// ------------------------------------------------------------------ TOOLS

bool Game::isFree(double x, double y, const CpuBmp* bmp, std::optional<uint32_t> col) const {
    if (!bmp) bmp = &bmpGround;
    double lim = HEIGHT - bmp->h;
    if (y < lim) return true;
    int px = hx::Int(x), py = hx::Int(y - lim);
    uint32_t pcol = bmp->get(px, py);
    if (col) return *col != pcol;
    return (int)(pcol >> 24) < 100;
}

bool Game::isLandingFree(double x, double y, std::optional<uint32_t> col) const {
    return isFree(x, y) && isFree(x, y, &bmpDecor, col);
}

int Game::getGround(double x) const {
    int y = HEIGHT - (pl ? pl->gMax : 0);
    while (true) {
        if (!isFree(x, y) || y >= HEIGHT) return y - 1;
        y++;
    }
}

// ------------------------------------------------------------------ INTER

void Game::initInter() {
    mcInter = dm->empty(DP_INTER);
    mcInter->setX(Cs::mcw);
    mt::DepthManager idm(mcInter);
    mcInterFuel = idm.attach("mcInterFuel", 0);
}

void Game::incMinerai(int n) { min += n; }
void Game::incCaps(int n) { chs += n; }

// ------------------------------------------------------------------ ENDING

void Game::initEnding(bool fl) {
    Module::initEnding(fl);
    step = End;
    if (pad && !pad->dead) pad->kill();
}

void Game::updateEnding() { Module::updateEnding(); }

void Game::endGame() {
    Shell::get()->endLander(flVictory, min - debit, missile, chs, travel ? &*travel : nullptr, item, flMarkHouse);
}

void Game::togglePause() {
    if (step != Play) return;
    Module::togglePause();
}

// ------------------------------------------------------------------ OUTRO (landing on Earth)

static void grey(flash::DisplayObject* mc, double c) {
    const double m0[20] = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0};
    const double r = 0.25, g = 0.15, b = 0.6;
    const double m1[20] = {r, g, b, 0, 0, r, g, b, 0, 0, r, g, b, 0, 0, 0, 0, 0, 1, 0};
    double m[20];
    for (int i = 0; i < 20; i++) m[i] = m0[i] * (1 - c) + m1[i] * c;
    mc->filters = {flash::Filter::colorMatrix(m)};
}

void Game::initOutro() {
    step = Outro;
    fadeCoef = 0;
}

void Game::updateOutro() {
    if (fadeCoef < 1) {
        fadeCoef = std::min(fadeCoef + 0.05 * mt::Timer::tmod, 1.0);
        grey(root.get(), fadeCoef);
        if (fadeCoef == 1) {
            // a still of the greyed landing framed on the table, then the story's ending scrolls up
            bmpScreen = std::make_shared<flash::BitmapData>(flash::Stage::get(), Cs::mcw, Cs::mch, false, 0);
            bmpScreen->draw(root.get(), Matrix());
            dm->destroy();
            root->filters.clear();
            table = dm->attach("mcTable", 1);
            if (MC s = table->smc())
                if (MC s2 = s->smc()) s2->attachBitmap(bmpScreen, 10);
            std::string txt = Text::str("OUTRO_0");
            if (Cs::pi->gotItem(MissionInfo::BADGE_FURI)) txt = Text::str("OUTRO_1");
            if (TF f = table->field("field")) {
                f->setText(txt);
                f->layout();
                f->setHeight(f->textHeight + 10);
            }
        }
        return;
    }
    TF f = table ? table->field("field") : nullptr;
    if (!f) return;
    double lim = 40 - f->textHeight;
    if (f->y > lim) {
        double coef = flPress ? 6 : 1;
        f->setY(std::max(lim, f->y - coef * mt::Timer::tmod));
    } else if (f->y == lim) {
        f->setY(f->y - 1);
        const auto& a = Text::raw("OUTRO_2");
        for (int i = 0; i < 2 && a.is_array() && i < (int)a.size(); i++) {
            MC mc = dm->attach("mcChoice", 5);
            mc->setY(65 + 150 * i);
            auto s = [&](int k) { return a[i].size() > (size_t)k && a[i][k].is_string() ? a[i][k].get<std::string>() : std::string(); };
            if (TF t = mc->field("fieldTitle")) t->setText(s(0));
            std::string txt = s(1);
            if (i == 0 && !Cs::pi->gotItem(MissionInfo::MODE_DIF)) txt += "\n<b>" + s(2) + "</b>";
            if (i == 1 && !Cs::pi->gotItem(MissionInfo::EARTH_PASS)) txt += "\n<b>" + s(2) + "</b>";
            if (TF t = mc->field("field")) {
                t->setHtmlText(txt);
                t->layout();
                t->setY(60 - t->textHeight * 0.5);
            }
            flash::MovieClip* raw = mc.get();
            if (MC sm = mc->smc()) sm->setAlpha(20);
            mc->onRollOver = [raw] { if (MC sm = raw->smc()) sm->setAlpha(60); };
            mc->onRollOut = [raw] { if (MC sm = raw->smc()) sm->setAlpha(20); };
            mc->onDragOver = mc->onRollOver;
            mc->onDragOut = mc->onRollOut;
            mc->onPress = [this, i] { choice(i); };
            mc->useHandCursor = true;
        }
    }
}

void Game::choice(int n) {
    int itemId = -1;
    if (n == 0) itemId = MissionInfo::MODE_DIF;
    if (n == 1) itemId = MissionInfo::EARTH_PASS;
    if (Cs::pi->gotItem(itemId)) itemId = -1;
    Shell::get()->endStory(n, itemId);
}

void Game::kill() {
    if (hero) hero->currentHouse = nullptr;
    Module::kill();
}

void Game::initKeyListener() {
    kl = std::make_shared<flash::Listener>();
    kl->onKeyDown = [this] { pressKey(); };
    flash::Stage::get()->keyListeners.push_back(kl);
}

void Game::pressKey() {
    int n = flash::Stage::get()->lastKey;
    if (n == 80 || n == 27) togglePause();
}

// ================================================================== Pix

void Pix::updatePos() {
    if (!root) return;
    root->setX(x);
    root->setY(y);
}

void Pix::kill() {
    if (root) root->removeMovieClip();
}

void Pix::movePoint(Pix* p, int sens, int depth) {
    if (depth > 4) return;  // walled in on every side (the original would recurse forever)
    int fwd = Cs::getDir(p->gid - sens);
    const int* d = Cs::DIR[fwd];
    const int* gd = Cs::DIR[p->gid];
    int nx = p->x + d[0], ny = p->y + d[1];
    Game* g = Game::me;
    if (!g->isLandingFree(nx, ny, 0xFFFF0000u)) {
        p->gid = fwd;
        movePoint(p, sens, depth + 1);
    } else {
        int gx = nx + gd[0], gy = ny + gd[1];
        if (g->isLandingFree(gx, gy, 0xFFFF0000u)) {
            p->x = gx;
            p->y = gy;
            p->gid = Cs::getDir(p->gid + sens);
        } else {
            p->x = nx;
            p->y = ny;
        }
    }
}

double Pix::getNormal(Pix* p, int range) {
    if (!p) p = this;
    std::vector<int> a;
    for (int i = 0; i < 2; i++) {
        int sens = i * 2 - 1;
        Pix np(p->x, p->y, gid);
        for (int n = 0; n < range; n++) {
            movePoint(&np, sens);
            a.push_back(np.gid);
        }
    }
    int dx = 0, dy = 0;
    for (int di : a) {
        dx += Cs::DIR[di][0];
        dy += Cs::DIR[di][1];
    }
    return Num::hMod(std::atan2((double)dy, (double)dx) + 3.14, 3.14);
}

bool Pix::isFree(int x, int y) {
    int ma = 2;
    if (x < ma || x > Game::WIDTH - ma) return false;
    return Game::me->isLandingFree(x, y, 0xFFFF0000u);
}

// ================================================================== PixPhys

PixPhys::PixPhys(MC mc) : Pix(0, 0, 1, std::move(mc)) {}

void PixPhys::fly() {
    if (!hx::isNull(frict)) {
        vx *= frict;
        vy *= frict;
    }
    if (!hx::isNull(weight)) vy += weight;
    parc = 1;
    vvx = vx;
    vvy = vy;
    int guard = 0;
    while (parc > 0 && guard++ < 4000) {
        double cx, cy;
        if (vvx > 0) cx = (1 - ox) / vvx;
        else if (vvx < 0) cx = ox / vvx;
        else cx = INFINITY;
        if (vvy > 0) cy = (1 - oy) / vvy;
        else if (vvy < 0) cy = oy / vvy;
        else cy = INFINITY;
        double c;
        int sx = 0, sy = 0;
        bool hasSx = false, hasSy = false;
        auto toInt = [](double v) { return std::isfinite(v) ? (int)v : 0; };
        if (std::abs(cx) < std::abs(cy)) {
            c = std::abs(cx);
            sx = toInt(cx / c);
            if (c == 0) sx = -1;
            hasSx = true;
        } else {
            c = std::abs(cy);
            sy = toInt(cy / c);
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
                if (isFree(x + sx, y)) {
                    x += sx;
                    ox -= sx;
                } else {
                    onBounce(sx, 0);
                }
            }
            if (hasSy) {
                if (isFree(x, y + sy)) {
                    y += sy;
                    oy -= sy;
                } else {
                    onBounce(0, sy);
                }
            }
        }
    }
    updatePos();
}

void PixPhys::onBounce(int sx, int sy) {
    if (flAngleBounce) {
        gid = getDir(sx, sy);
        double n = getNormal();
        double a = std::atan2(vy, vx) + 3.14;
        double da = Num::hMod(a - n, 3.14);
        double dist = std::sqrt(vx * vx + vy * vy) * colFrict;
        double na = Num::hMod(n - da, 3.14);
        vx = std::cos(na) * dist;
        vy = std::sin(na) * dist;
        ox = 0.5;
        oy = 0.5;
        parc = 0;
        return;
    }
    if (sx != 0) {
        vvx *= -colFrict;
        vx *= -colFrict;
    }
    if (sy != 0) {
        vvy *= -colFrict;
        vy *= -colFrict;
    }
}

void PixPhys::updatePos() {
    if (!root) return;
    root->setX(x + ox);
    root->setY(y + oy);
}

int PixPhys::getDir(int x, int y) const {
    if (x == 1 && y == 0) return 0;
    if (x == 0 && y == 1) return 1;
    if (x == -1 && y == 0) return 2;
    if (x == 0 && y == -1) return 3;
    return 0;
}

// ================================================================== Mineral

Mineral::Mineral(mt::OldRandom& seed) {
    Game::me->minerals.push_back(this);
    root = Game::me->gdm->attach("mcCrystal", Game::DP_MINERALS);
    if (MC s = root->smc()) s->setRotation(seed.rand() * 360);
}

void Mineral::setValue(int n) {
    int skinId = 0;
    if (n > 35) skinId++;
    if (n > 200) skinId++;
    val = n;
    life = 30 + n * 10;
    if (MC s = root->smc()) {
        double sc = 60 + n * (5.0 / (skinId * 7 + 1));
        s->setXScale(sc);
        s->setYScale(sc);
        s->gotoAndStop(skinId + 1);
    }
}

void Mineral::dropToSurface() { root->setY(Game::me->getGround(root->x)); }

void Mineral::consume(double n) {
    life -= n;
    if (life <= 0) collect();
}

void Mineral::collect() {
    Game* g = Game::me;
    int max = hx::Int(Num::mm(3, val, 50));
    for (int i = 0; i < max; i++) {
        flash::Point pos = getPosModifier();
        auto* p = new mt::Phys(g->bdm->attach("partTwinkle", Game::DP_PARTS));
        p->x = root->x + pos.x;
        p->y = root->y + pos.y;
        p->timer = 10 + hx::random() * 10;
        p->fadeType = 0;
        p->setScale(50 + hx::random() * 100);
        p->sleep = hx::random() * 5;
        p->vy -= hx::random();
        p->root->blendMode = flash::Blend::Add;
        p->root->gotoAndPlay(hx::random(2) + 1);
        p->updatePos();
    }
    kill();
}

void Mineral::kill() {
    if (flDeath) return;
    flDeath = true;
    Game* g = Game::me;
    hx::remove(g->minerals, this);
    if (g->pad) hx::remove(g->pad->minList, this);
    root->removeMovieClip();
}

flash::Point Mineral::getPosModifier() {
    MC b = Game::me->base;
    MC s = root->smc();
    int to = 0;
    while (true) {
        flash::Rect r = root->getBounds(b.get());
        double x = (hx::random() - 0.5) * (r.xMax - r.xMin);
        double y = -hx::random() * (r.yMax - r.yMin) * 0.5;
        flash::Point gp = b->localToGlobal({x + root->x, y + root->y});
        if (s && s->hitTest(gp.x, gp.y, true)) return {x, y};
        if (to++ > 20) return {0, 0};
    }
}

// ================================================================== Drone

Drone::Drone() : Phys(Game::me->bdm->attach("mcDrone", Game::DP_DRONES)) {
    pad = Game::me->pad;
    frame = 0;
    root->stop();
}

void Drone::update() {
    Phys::update();
    if (dead) return;
    if (jumpSens != 0) updateJump(jumpSens);
    else updateConsume();
}

void Drone::initModeSeeker() {
    x = pad->x;
    y = pad->y;
    jumpToMineral();
}

void Drone::getMinPos() {
    dec = min->getPosModifier();
    tx = min->root->x + dec.x;
    ty = min->root->y + dec.y;
}

void Drone::jumpToMineral() {
    jumpSens = 1;
    min = pad->minList[hx::random((int)pad->minList.size())];
    c = 0;
    getMinPos();
}

void Drone::updateJump(int sens) {
    double ox = x, oy = y;
    c = Num::mm(0, c + sens * 0.05, 1);
    x = pad->x * (1 - c) + tx * c;
    y = pad->y * (1 - c) + ty * c - std::sin(c * 3.14) * 40;
    // QUEUE
    MC mc = Game::me->bdm->attach("mcQueueDrone", Game::DP_GROUND);
    mc->setX(ox);
    mc->setY(oy);
    double dx = x - ox, dy = y - oy;
    mc->setRotation(std::atan2(dy, dx) / 0.0174);
    mc->setXScale(std::sqrt(dx * dx + dy * dy));
    root->setRotation(std::atan2(dy, dx) / 0.0174);
    if (c == 1) jumpSens = 0;
    if (c == 0) {
        pad->receiveDrone(this);
        kill();
    }
}

void Drone::updateConsume() {
    double coef = 0.1;
    double dx = tx - x, dy = ty - y;
    x += dx * coef;
    y += dy * coef;
    double move = std::abs(dx) + std::abs(dy);
    if (hx::random(14) == 0 || move < 1) getMinPos();
    frame = std::fmod(frame + move * 0.25, 12);
    root->gotoAndStop(hx::Int(frame) + 1);
    root->setRotation(std::atan2(dy, dx) / 0.0174);
    if (!min->flDeath) min->consume(1);
    if (min->flDeath) {
        hasMinVal = true;
        minVal = min->val;
        min->val = 0;
        tx = x;
        ty = y;
        jumpSens = -1;
    }
}

// ================================================================== Pad

Pad::Pad() : PadSkin(Game::me->bdm->attach("mcPad", Game::DP_PAD)) {
    Game* g = Game::me;
    setRay(Cs::pi->getRay());
    setType(0);
    weight = WEIGHT * g->pl->g;
    fuel = 100;
    landAngleMax = 0.5;
    const auto& si = Cs::pi->shopItems;
    landPodLength = 0;
    if (si.get(ShopInfo::PODS) == 1) landPodLength = 14;
    if (si.get(ShopInfo::PODS_EXTEND_0) == 1) landPodLength += 10;
    if (si.get(ShopInfo::PODS_EXTEND_1) == 1) landPodLength += 10;
    if (si.get(ShopInfo::PODS_EXTEND_2) == 1) landPodLength += 10;
    if (Game::debugKit() && landPodLength == 0) landPodLength = 14;
    acc = 0.15;
    if (si.get(ShopInfo::LANDER_REACTOR_0) == 1) acc += 0.15;
    if (si.get(ShopInfo::LANDER_REACTOR_1) == 1) acc += 0.2;
    if (si.get(ShopInfo::LANDER_REACTOR_2) == 1) acc += 0.3;
    droneActionRay = 100;
    fuelConsume = 0.3;
    drones = Cs::pi->drone;
    angle = -1.57;
    step = Fly;
    setReactor(true);
    if (MC s = mcReactor->smc()) s->visible = false;
}

void Pad::update() {
    Phys::update();
    if (dead) return;
    switch (step) {
        case Fly: updateFly(); break;
        case Land: updateLanding(); break;
    }
    if (dead) return;
    checkCols();
}

void Pad::checkCols() {
    Game* g = Game::me;
    double rh = 5;
    double sx = x - std::cos(angle) * rh, sy = y - std::sin(angle) * rh;
    double a = angle + 1.57;
    double ca = std::cos(a), sa = std::sin(a);
    int max = 8;
    double mr = ray - 7;
    for (int i = 0; i < max; i++) {
        double c = ((double)i / (max - 1)) * 2 - 1;
        int px = hx::Int(sx + ca * c * mr), py = hx::Int(sy + sa * c * mr);
        if (!g->isFree(px, py, &g->bmpCol)) {
            explode(nullptr);
            return;
        }
    }
}

void Pad::takeOff() {
    step = Fly;
    weight = WEIGHT * Game::me->pl->g;
    vx = 0;
    vy = -2 * Game::me->pl->g;
    removePods();
    navi::Map::me->removeMenu(7);
}

void Pad::updateFly() {
    control();
    if (step != Fly) return;
    checkLanding();
    double rec = 1;
    if (x < 0) {
        vx *= 0.6;
        vx += rec;
    }
    if (x > Game::WIDTH) {
        vx *= 0.6;
        vx -= rec;
    }
    if (y > Game::HEIGHT + 30) explode(nullptr);
}

void Pad::control() {
    double ta = getMouseAngle();
    if (y < 150) ta = -1.57;
    double da = Num::hMod(ta - angle, 3.14);
    angle += da * 0.1;
    root->setRotation(angle / 0.0174 + 90);
    bool flThrust = Game::me->flPress && fuel > 0;
    if (flThrust) thrust();
    if (MC s = mcReactor->smc()) s->visible = flThrust;
}

void Pad::thrust() {
    double ca = std::cos(angle), sa = std::sin(angle);
    vx += ca * acc;
    vy += sa * acc;
    incFuel(-fuelConsume * mt::Timer::tmod);
    double ec = 16;
    double sp = 2 + hx::random() * 8;
    auto* p = new mt::Phys(Game::me->bdm->attach("partReactorSpark", Game::DP_UNDERPARTS));
    double a = hx::random() * 6.28;
    double d = 2 + hx::random() * 16;
    p->x = (x - ca * ec) - std::cos(a) * d;
    p->y = (y - sa * ec) - std::sin(a) * d;
    p->vx = vx - ca * sp;
    p->vy = vy - sa * sp;
    if (MC s = p->root->smc()) s->setX(d);
    p->root->setRotation(a / 0.0174);
    p->vr = (hx::random() * 2 - 1) * 20;
    p->fr = 0.95;
    p->fadeType = 0;
    p->timer = 10 + hx::random() * 10;
    p->setScale(50 + hx::random() * 100);
    Filt::glow(p->root.get(), 16, 6, 0xFFFF44);
    p->root->blendMode = flash::Blend::Add;
}

void Pad::incFuel(double inc) {
    fuel = Num::mm(0, fuel + inc, 100);
    if (MC f = Game::me->mcInterFuel)
        if (MC s = f->smc()) s->setYScale(fuel);
}

void Pad::checkLanding() {
    if (landPodLength == 0) return;
    double da = Num::hMod(angle + 1.57, 3.14);
    if (da > landAngleMax) return;
    double r = ray - MARGIN;
    double bx = x - std::cos(angle) * HR, by = y - std::sin(angle) * HR;
    double a[2][2], ga[2][2];
    for (int i = 0; i < 2; i++) {
        int sens = i * 2 - 1;
        double an = angle - 1.57 * sens;
        a[i][0] = bx + std::cos(an) * r;
        a[i][1] = by + std::sin(an) * r;
    }
    for (int i = 0; i < 2; i++) {
        double n = getPodPos(a[i][0], a[i][1]);
        if (hx::isNull(n)) return;
        ga[i][0] = a[i][0];
        ga[i][1] = a[i][1] + n;
    }
    // INIT LANDING
    Game* g = Game::me;
    pods.clear();
    int sens = -1;
    for (int id = 0; id < 2; id++) {
        Pod pod;
        pod.mc = g->bdm->attach("mcLandPod", Game::DP_PAD);
        pod.mc->setX(ga[id][0]);
        pod.mc->setY(ga[id][1]);
        pod.p = new mt::Phys(g->dm->empty(Game::DP_PAD));
        pod.p->x = a[id][0];
        pod.p->y = a[id][1];
        pod.p->vx = vx;
        pod.p->vy = vy;
        pod.p->frict = 0.95;
        pod.sens = sens;
        Filt::glow(pod.mc.get(), 2, 4, 0xA8780B);
        pods.push_back(pod);
        sens += 2;
    }
    g->bdm->over(root);
    initLanding();
}

void Pad::initLanding() {
    step = Land;
    weight = 0;
    if (MC s = mcReactor->smc()) s->visible = false;
    landTimer = 10;
    initLandingControl();
    minList.clear();
    for (auto* m : Game::me->minerals) {
        double dx = std::max(std::abs(x - m->root->x) - ray, 0.0);
        double dy = y - m->root->y;
        if (std::sqrt(dx * dx + dy * dy) < droneActionRay) minList.push_back(m);
    }
}

void Pad::initLandingControl() {
    Game::me->focus = [this] { return flash::Point{x, y}; };
    controlType = 0;
    if (Game::me->pl->type == 1 || Cs::pi->gotItem(MissionInfo::COMBINAISON) || Game::debugKit())
        navi::Map::me->newMenu(7, [this] { dropHero(); });
}

void Pad::updateLanding() {
    updatePods();
    if (!minList.empty() && drones > 0) {
        if (hx::isNull(droneTimer)) droneTimer = 0;
        droneTimer -= mt::Timer::tmod;
        if (droneTimer < 0) {
            droneTimer = 10;
            drones--;
            auto* d = new Drone();
            d->initModeSeeker();
        }
    }
    checkTakeOff();
}

void Pad::checkTakeOff() {
    if (controlType != 0) return;
    if (hx::isNull(landTimer)) {
        double da = Num::hMod(getMouseAngle() + 1.57, 3.14);
        if (std::abs(da) < 0.77 && Game::me->flClick) takeOff();
    } else {
        landTimer -= mt::Timer::tmod;
        if (landTimer < 0) landTimer = hx::NUL;
    }
}

void Pad::dropHero() {
    if (controlType == 1 || step != Land) return;
    controlType = 1;
    Hero* h = Game::me->newHero();
    h->x = hx::Int(x);
    h->y = hx::Int(y);
    navi::Map::me->removeMenu(7);
}

double Pad::getPodPos(double px0, double py0) {
    int n = 1;
    while (true) {
        int px = hx::Int(px0), py = hx::Int(py0 + n);
        if (!Game::me->isLandingFree(px, py)) return n;
        if (n++ >= landPodLength) return hx::NUL;
    }
}

void Pad::removePods() {
    for (auto& p : pods) p.mc->removeMovieClip();
    pods.clear();
}

void Pad::updatePods() {
    if (pods.size() < 2) return;
    for (auto& pd : pods) {
        double tx = pd.mc->x, ty = pd.mc->y - (landPodLength + 3);
        double dx = tx - pd.p->x, dy = ty - pd.p->y;
        double c = 0.1;
        pd.p->vx += dx * c;
        pd.p->vy += dy * c;
    }
    // CONTRAINTE
    double dx = pods[0].p->x - pods[1].p->x, dy = pods[0].p->y - pods[1].p->y;
    double dist = std::sqrt(dx * dx + dy * dy);
    double a = std::atan2(dy, dx);
    double ca = std::cos(a), sa = std::sin(a);
    double dd = (dist - (ray - MARGIN) * 2) * 0.5;
    for (int id = 0; id < 2; id++) {
        int sens = id * 2 - 1;
        pods[id].p->x += ca * dd * sens;
        pods[id].p->y += sa * dd * sens;
    }
    // POSITION
    angle = a - 1.57;
    root->setRotation(angle / 0.0174 + 90);
    x = (pods[0].p->x + pods[1].p->x) * 0.5 + std::cos(angle) * HR;
    y = (pods[0].p->y + pods[1].p->y) * 0.5 + std::sin(angle) * HR;
    updatePos();
    // FOOT DRAW
    double h = landPodLength + 10;
    for (auto& pd : pods) {
        MC mc = pd.mc;
        mc->clear();
        mc->lineStyle(3, 0xE9B849, 100);
        double ddx = pd.p->x - mc->x, ddy = pd.p->y - mc->y;
        double d = std::sqrt(ddx * ddx + ddy * ddy) * 0.5;
        double leg = h * 0.5;
        double r = std::sqrt(leg * leg - d * d);
        if (!(r > 0)) r = 0;
        double an = std::atan2(ddy, ddx) + 1.57;
        double cx = ddx * 0.5 - std::cos(an) * r * pd.sens;
        double cy = ddy * 0.5 - std::sin(an) * r * pd.sens;
        mc->moveTo(0, 0);
        mc->lineTo(cx, cy);
        mc->lineTo(ddx, ddy);
        if (MC s = mc->smc()) {
            s->setX(cx);
            s->setY(cy);
        }
    }
}

void Pad::receiveDrone(Drone* d) {
    if (d->hasMinVal) {
        showMinerai(d->minVal);
        Game::me->incMinerai(d->minVal);
    }
    droneTimer = 10;
    drones++;
    for (auto& pd : pods) pd.p->vy = 1.5;
}

void Pad::receiveHero() {
    Game* g = Game::me;
    Hero* hero = g->hero;
    if (!hero) return;
    initLandingControl();
    if (hero->min > 0) {
        showMinerai(hero->min);
        g->incMinerai(hero->min);
    }
    for (auto& pd : pods) {
        pd.p->vx = hero->vx * 0.33;
        pd.p->vy = hero->vy * 0.33;
    }
    hero->kill();
}

void Pad::showMinerai(int n) {
    auto* p = new mt::Phys(Game::me->bdm->attach("mcMinCounter", Game::DP_INTER));
    p->x = x;
    p->y = y - 40;
    p->timer = 15;
    p->fadeLimit = 6;
    p->weight = -0.1;
    Filt::glow(p->root.get(), 2, 4, 0);
    if (TF f = p->root->field("field")) {
        f->setText(std::to_string(n));
        f->layout();
        p->x += (16 + f->textWidth) * 0.5;
    }
}

double Pad::getMouseAngle() const {
    flash::Point m = Game::me->base->mouse();
    return std::atan2(m.y - y, m.x - x);
}

void Pad::explode(MC bmc) {
    if (dead) return;
    if (!bmc) bmc = Game::me->bdm->empty(Game::DP_UNDERPARTS);
    PadSkin::explode(bmc);
    Game::me->initEnding(false);
}

void Pad::kill() {
    if (dead) return;
    removePods();
    if (navi::Map::me) navi::Map::me->removeMenu(7);
    if (Game::me && Game::me->pad == this) {
        // the camera stays where the pad was
        double fx = x, fy = y;
        Game::me->focus = [fx, fy] { return flash::Point{fx, fy}; };
    }
    PadSkin::kill();
}

// ================================================================== Hero

Hero::Hero(int id) : PixPhys(Game::me->bdm->attach("mcHero", Game::DP_HERO)) {
    flControl = true;
    acc = 0.7;
    initFly();
    colFrict = 0.5;
    min = 0;
    flLandingReady = false;
    skinId = id;
    playAnim("fly");
}

bool Hero::isFree(int x, int y) { return Pix::isFree(x, y); }

void Hero::update() {
    switch (step) {
        case Fly: updateFly(); break;
        case Land: updateLanding(); break;
    }
    if (currentHouse) currentHouse->update();
    checkMinCol();
    updateHouse();
    updateIcon();
    int m = 10;
    if (x < m || x > Game::WIDTH - m) {
        x = hx::Int(Num::mm(m, x, Game::WIDTH - m));
    } else {
        if (!isFree(x, y)) recalY();
    }
}

void Hero::playAnim(const std::string& label) {
    root->gotoAndStop(label);
    MC skin = root->smc();
    if (!skin) return;
    for (const char* n : {"j1", "j2", "f1", "f2", "body", "h", "b1", "b2"})
        if (MC part = skin->clip(n)) part->gotoAndStop(skinId + 1);
}

void Hero::initFly() {
    step = Fly;
    weight = WEIGHT * Game::me->pl->g;
    root->setXScale(100);
    ox = 0.5;
    oy = 0.5;
}

void Hero::updateFly() {
    bool flThrust = false;
    double sens = 1;
    Game* g = Game::me;
    if (flJump) {
        if (g->flClick && skinId == 0 && flControl) {
            flJump = false;
            playAnim("fly");
        }
        if (vx != 0) sens = std::abs(vx) / vx;
    } else {
        double ta = std::atan2(vy, vx);
        if (g->flPress && flControl) {
            if (fuel > 0) {
                weight = WEIGHT;
                fuel -= mt::Timer::tmod;
                ta = getMouseAngle();
                vx += std::cos(ta) * acc;
                vy += std::sin(ta) * acc;
                flThrust = true;
            }
        }
        if (!flThrust) weight = WEIGHT * g->pl->g;
        // face the way it goes
        double ca = std::cos(ta);
        sens = ca == 0 ? 1 : ca / std::abs(ca);
        ta = ta + 1.57 - 1.57 * sens;
        double da = Num::hMod((ta / 0.0174) - root->rotation, 180);
        double c = flThrust ? 0.3 : 0.04;
        root->setRotation(root->rotation + Num::hMod(da * c, 180));
    }
    root->setXScale(sens * 100);

    if (flThrust) {
        double ba = root->rotation * 0.0174 + 1.57 - 1.57 * sens;
        double ca = std::cos(ba), sa = std::sin(ba);
        double ec = 16;
        double sp = 1 + hx::random() * 4;
        auto* p = new mt::Phys(g->bdm->attach("partReactorSpark", Game::DP_UNDERPARTS));
        double a = hx::random() * 6.28;
        double d = 2 + hx::random() * 18;
        p->x = (x - ca * ec) - std::cos(a) * d;
        p->y = (y - sa * ec) - std::sin(a) * d;
        p->vx = vx - ca * sp;
        p->vy = vy - sa * sp;
        if (MC s = p->root->smc()) s->setX(d);
        p->root->setRotation(a / 0.0174);
        p->vr = (hx::random() * 2 - 1) * 10;
        p->fr = 0.95;
        p->fadeType = 0;
        p->timer = 10 + hx::random() * 10;
        p->setScale(50 + hx::random() * 80);
    }

    fly();
    if (step != Fly) return;  // landed while flying
    updatePos();

    double lim = 200;
    if (y < lim) {
        double c = y / lim;
        vy += (1 - c) * 3;
        vy *= c;
    }

    // back into the pad
    if (vy > 0) {
        if (flLandingReady && g->pad && !g->pad->dead) {
            flash::Point gp = g->base->localToGlobal({(double)x, (double)y});
            if (g->pad->root->hitTest(gp.x, gp.y, true)) g->pad->receiveHero();
        }
    } else {
        flLandingReady = true;
    }
}

void Hero::recalY() {
    int guard = 0;
    while (isFree(x, y + 1) && y < Game::HEIGHT + 50 && guard++ < 2000) y++;
    guard = 0;
    while (!isFree(x, y) && guard++ < 2000) y--;
}

void Hero::onBounce(int sx, int sy) {
    PixPhys::onBounce(sx, sy);
    double vit = std::sqrt(vx * vx + vy * vy);
    vx *= 0.5;
    vy *= 0.5;
    if (checkBalance())
        if (vit < 6 || flJump) initLanding();
}

bool Hero::checkBalance() {
    double eq = Num::hMod((-1.57 - getNormal()), 3.14);
    return std::abs(eq) < 1.57;
}

void Hero::initLanding() {
    root->setYScale(100);
    step = Land;
    root->setRotation(0);
    vx = 0;
    vy = 0;
    weight = 0;
    parc = 0;
    run = 0;
    frame = 0;
    flWalk = false;
    fuel = 100;
    recalY();
    playAnim("stand");
    updatePos();
    if (!flControl) Game::me->initOutro();
}

void Hero::updateLanding() {
    Game* g = Game::me;
    if (g->flPress && flControl) {
        updateWalk();
    } else if (flWalk) {
        MC s = root->smc();
        if (s) {
            int fr = s->currentFrame();
            if (fr < 5 || (fr >= 17 && fr < 22)) s->gotoAndStop(1);
            else s->play();
        }
        flWalk = false;
    }
    if (step == Land) checkJump();
}

void Hero::updateWalk() {
    double dx = Game::me->base->mouse().x - x;
    double sens = dx != 0 ? dx / std::abs(dx) : 1;
    if (std::abs(dx) < 30) dx = 0;
    double c = Num::mm(-2, dx / 60, 2);
    if (flWalk && c == 0) {
        flWalk = false;
        frame = 0;
        playAnim("stand");
    }
    if (!flWalk && c != 0) {
        flWalk = true;
        playAnim("run");
    }
    run += c * WALK_SPEED;
    frame = Num::sMod(frame + c * WALK_SPEED * 0.5, 33);
    if (MC s = root->smc()) s->gotoAndStop(hx::Int(frame) + 1);
    root->setXScale(100 * sens);
    while (run > 1 && step == Land) {
        run--;
        walk(1);
    }
    while (run < -1 && step == Land) {
        run++;
        walk(-1);
    }
    updatePos();
}

void Hero::walk(int sens) {
    Pix::movePoint(this, sens);
    if (!checkBalance()) {
        Pix::movePoint(this, -sens);
        double a = getNormal();
        double speed = 1;
        vx = std::cos(a) * speed;
        vy = std::sin(a) * speed;
        initFly();
    }
}

void Hero::checkJump() {
    double a = getMouseAngle();
    double da = Num::hMod(a + 1.57, 3.14);
    double speed = 8;
    if (std::abs(da) < 0.77 && Game::me->flClick && flControl) {
        vx = std::cos(a) * speed;
        vy = std::sin(a) * speed;
        flJump = true;
        initFly();
        playAnim("jump");
    }
}

void Hero::setIcon(const std::string& label) {
    if (!icon || icon->dead) {
        icon = new mt::Phys(Game::me->bdm->attach("mcThink", Game::DP_FOREGROUND));
        icon->fadeType = 0;
    }
    icon->timer = 60;
    icon->setScale(100);
    if (MC s = icon->root->smc()) s->gotoAndStop(label);
}

void Hero::updateIcon() {
    if (!icon) return;
    if (icon->dead) {
        icon = nullptr;
        return;
    }
    icon->x = x;
    icon->y = y - 15;
    icon->updatePos();
    if (icon->timer <= 0) icon = nullptr;
}

void Hero::updateHouse() {
    double r = 75;
    Game* g = Game::me;
    if (!currentHouse) {
        for (auto* h : g->houses) {
            double dx = x - h->x, dy = y - h->y;
            if (std::abs(dx) + std::abs(dy) < r && std::sqrt(dx * dx + dy * dy) < r) {
                currentHouse = h;
                h->active();
            }
        }
    } else {
        House* h = currentHouse;
        double dx = x - h->x, dy = y - h->y;
        if (std::sqrt(dx * dx + dy * dy) > r + 10) {
            currentHouse = nullptr;
            h->unactive();
        }
    }
}

void Hero::checkMinCol() {
    auto list = Game::me->minerals;
    for (auto* m : list) {
        double dx = x - m->root->x, dy = y - m->root->y;
        double c = 0.5;
        if (std::abs(dx) < m->root->width() * c && std::abs(dy) < m->root->height() * c) {
            int v = m->val;
            m->collect();
            min += v;
        }
    }
}

double Hero::getMouseAngle() const {
    flash::Point m = Game::me->base->mouse();
    return std::atan2(m.y - y, m.x - x);
}

void Hero::kill() {
    if (currentHouse) {
        currentHouse->unactive();
        currentHouse = nullptr;
    }
    Pix::kill();
    Game::me->retireHero(this);
}

// ================================================================== House

House::House(int rnd, Scenario s, int item) : scn(s), itemId(item), seed(rnd), eventSeed(rnd) {
    game = Game::me;
    game->houses.push_back(this);
    int ma = 100;
    MC mc = game->dm->attach("mcHouse", 0);
    x = hx::Int(ma + seed.rand() * (Game::WIDTH - ma * 2));
    y = game->getGround(x);
    type = 0;
    int my = y - (Game::HEIGHT - game->pl->dMax);
    int a[2];
    for (int n = 0; n < 2; n++) a[n] = game->getGround(x + (n * 2 - 1) * 80);
    int dx = a[1] - a[0];
    sx = dx == 0 ? 1 : dx / std::abs(dx);
    Matrix m;
    m.scale(sx, 1);
    m.translate(x, my);
    game->drawClip(game->bmpForeground, mc.get(), m);
    int fr = scn == Scenario::Locked ? 2 : 3;
    if (MC s2 = mc->smc()) s2->gotoAndStop(fr);
    game->drawClip(game->bmpDecor, mc.get(), m);
    mc->removeMovieClip();
}

void House::dig() {
    MC mc = game->dm->attach("mcHouse", 0);
    if (MC s = mc->smc()) s->gotoAndStop(4);
    Matrix m;
    m.scale(sx, 1);
    m.translate(x, y - (Game::HEIGHT - game->pl->gMax));
    game->drawClip(game->bmpGround, mc.get(), m, flash::Blend::Erase);
    mc->removeMovieClip();
}

void House::update() {
    Hero* h = Game::me->hero;
    if (alienBehaviour == 0 && h && mcAlien) {
        double dx = h->x - mcAlien->x, dy = h->y - mcAlien->y;
        double dist = std::sqrt(dx * dx + dy * dy);
        double a = std::atan2(dy, dx);
        double d = 3 + std::sqrt(h->vx * h->vx + h->vy * h->vy);
        mcAlien->setX(mcAlien->x + std::cos(a) * d);
        mcAlien->setY(mcAlien->y + std::sin(a) * d);
        mcAlien->setAlpha(Num::mm(0, dist * 5, 100));
        if (dist < 6) {
            alienBehaviour = hx::NUL;
            mcAlien->removeMovieClip();
            mcAlien = nullptr;
        }
    }
}

void House::active() {
    eventSeed = seed;
    switch (scn) {
        case Scenario::Empty: break;
        case Scenario::Locked:
            if (game->hero) game->hero->setIcon("lock");
            return;
        case Scenario::Treasure: {
            auto* m = new Mineral(seed);
            game->ownedMinerals.emplace_back(m);
            m->setValue(100 + seed.random(hx::Int(game->level->dst * 5)));
            m->root->setX(x + (eventSeed.rand() * 2 - 1) * 10);
            m->dropToSurface();
            mark();
            break;
        }
        default: initAlien(); break;
    }
    if (!mask) {
        mask = game->fdm->attach("mcHouse", 1);
        mask->setX(x);
        mask->setY(y);
        mask->gotoAndStop(type + 1);
        if (MC s = mask->smc()) s->gotoAndStop(4);
        mask->blendMode = flash::Blend::Erase;
        mask->setXScale(sx * 100);
    }
}

void House::unactive() {
    if (mask) mask->removeMovieClip();
    mask = nullptr;
    if (mcAlien) {
        mcAlien->removeMovieClip();
        mcAlien = nullptr;
        if (navi::Map::me) navi::Map::me->removeMenu(0);
    }
}

void House::mark() {
    scn = Scenario::Empty;
    if (navi::Map::me) navi::Map::me->removeMenu(0);
    game->flMarkHouse = true;
}

static const std::pair<Scenario, int> SCN_STATS[] = {
    {Scenario::Casino, 0}, {Scenario::Swapper, 0}, {Scenario::Treasure, 6}, {Scenario::Traveler, 15},
    {Scenario::Furi, 10},  {Scenario::Seller, 7},  {Scenario::Info, 8},     {Scenario::Empty, 10},
    {Scenario::Locked, 10}, {Scenario::Locked, 10},
};

Scenario House::getScenario(int zid, mt::OldRandom& seed, int& itemId) {
    itemId = -1;
    // GIVE ITEM: some houses hold a story item
    std::vector<int> a = {MissionInfo::SALMEEN_COUSIN, MissionInfo::BADGE_FURI, MissionInfo::BALL_DOUBLE,
                          MissionInfo::RETROFUSER, MissionInfo::KARBONITE, MissionInfo::COMBINAISON};
    for (int i = 0; i < 12; i++) a.push_back(MissionInfo::TBL_0 + i);
    for (int i = 0; i < 42; i++) a.push_back(MissionInfo::EMAP_0 + i);
    for (int n : a) {
        const ItemPos& p = MissionInfo::item(n);
        if (p.has && Cs::pi->x == p.x && Cs::pi->y == p.y) {
            Scenario s = navi::ItemGiver::getScn(n);
            if (s == Scenario::ItemGiver) itemId = n;
            return s;
        }
    }
    Game* g = Game::me;
    if (g->flHouseVisited) return Scenario::Empty;
    // DESERT / UNINHABITED
    double pop = g->pl->pop;
    if (hx::isNull(pop) || pop * seed.rand() < 1) return Scenario::None;
    int sum = 0;
    for (auto& o : SCN_STATS) sum += o.second;
    int rnd = seed.random(sum);
    sum = 0;
    for (auto& o : SCN_STATS) {
        sum += o.second;
        if (sum > rnd) return o.first;
    }
    return Scenario::None;
}

void House::initAlien() {
    mcAlien = game->bdm->attach("landerAlien", Game::DP_HERO);
    mt::OldRandom sc = seed;
    navi::Shop::initAlien(mcAlien.get(), &sc);
    mcAlien->setX(x + (eventSeed.rand() * 2 - 1) * 30);
    mcAlien->setY(game->getGround(x));
    mcAlien->setXScale(sx * 100);
    Filt::glow(mcAlien.get(), 2, 4, 0);
    mt::OldRandom ms = seed;
    navi::Map::me->newMenu(0, [this] { launchScreen(); }, &ms);
}

void House::launchScreen() {
    navi::Map* map = navi::Map::me;
    if (!map || map->menu || !mcAlien) return;
    double x = mcAlien->x + game->base->x - (mcAlien->width() + 16) * 0.5;
    double y = mcAlien->y + game->base->y - (mcAlien->height() + 16) * 0.5;
    mt::OldRandom s = seed;
    switch (scn) {
        case Scenario::Traveler: map->menu = std::make_unique<navi::Traveler>(x, y, &s); break;
        case Scenario::Seller: map->menu = std::make_unique<navi::Shop>(x, y); break;
        case Scenario::ItemGiver: map->menu = std::make_unique<navi::ItemGiver>(x, y, &s, itemId); break;
        case Scenario::Furi: map->menu = std::make_unique<navi::Furi>(x, y, &s); break;
        case Scenario::Info: map->menu = std::make_unique<navi::Info>(x, y, &s); break;
        default: break;
    }
}

}  // namespace lander

// ================================================================== the inhabitants' menus

namespace navi {

using lander::Game;

static std::string replaceAll(std::string s, const std::string& a, const std::string& b) {
    if (a.empty()) return s;
    size_t p = 0;
    while ((p = s.find(a, p)) != std::string::npos) {
        s.replace(p, a.size(), b);
        p += b.size();
    }
    return s;
}

static std::string pick(const std::string& key, mt::OldRandom& seed) {
    int n = Text::count(key);
    return Text::at(key, seed.random(std::max(1, n)));
}

static std::string pickJson(const nlohmann::json& a, mt::OldRandom& seed) {
    if (!a.is_array() || a.empty()) {
        seed.random(1);
        return "";
    }
    const auto& v = a[seed.random((int)a.size())];
    return v.is_string() ? v.get<std::string>() : std::string();
}

static std::string coord(int x, int y) { return "[" + std::to_string(x) + "][" + std::to_string(y) + "]"; }

// ------------------------------------------------------------------ People

void People::init() {
    Menu::init();
    skin = dm->attach("mcPeople", 1);
    Shop::initAlien(skin.get(), bs.get());
    skin->setX(MARGIN);
    skin->setY(MARGIN * ((double)Cs::mch / Cs::mcw));
}

void People::setText(const std::string& s) {
    txt = s;
    cursor = 0;
    TF f = skin ? skin->field("field") : nullptr;
    if (!f) return;
    f->setText(s);
    f->layout();
    f->setY(std::max(143.0, 216 - f->textHeight * 0.5));
    f->setText("");
}

void People::addBut(int id, std::function<void()> f) {
    if (!mcButs) {
        mcButs = dm->empty(2);
        butDm = std::make_unique<mt::DepthManager>(mcButs);
        mcButs->setY(317);
    }
    MC mc = butDm->attach("mcButPeople", 0);
    if (TF field = mc->field("field")) {
        field->setText(Text::at("BUTTON_PEOPLE", id));
        field->layout();
        field->setWidth(field->textWidth + 10);
        if (MC s = mc->smc()) s->setXScale(field->width());
    }
    mc->onPress = std::move(f);
    flash::MovieClip* raw = mc.get();
    mc->onRollOver = [raw] { raw->blendMode = flash::Blend::Add; };
    mc->onRollOut = [raw] { raw->blendMode = flash::Blend::Normal; };
    mc->useHandCursor = true;
    buts.push_back(mc);
    updateButPos();
}

void People::updateButPos() {
    double x = 0;
    for (auto& mc : buts) {
        mc->setX(x);
        x += mc->width() + 10;
    }
    mcButs->setX(Cs::mcw * 0.5 - mcButs->width() * 0.5);
}

void People::removeAllButs() {
    if (mcButs) mcButs->removeMovieClip();
    mcButs = nullptr;
    buts.clear();
}

void People::update() {
    Menu::update();
    if (cursor >= 0) {
        if (cursor < (int)txt.size()) {
            cursor++;
            while (cursor < (int)txt.size() && ((unsigned char)txt[cursor] & 0xC0) == 0x80) cursor++;  // whole UTF-8
        }
        std::string str = txt.substr(0, cursor);
        if (cursor % 3 > 0) str += "_";
        if (TF f = skin ? skin->field("field") : nullptr) {
            // field.scroll = maxscroll: keep the end of the text in view
            f->setText(str);
            f->layout();
            size_t cut = 0;
            while (f->textHeight > f->height_ && cut < str.size()) {
                size_t nl = str.find('\n', cut);
                size_t sp = str.find(' ', cut);
                size_t nx = std::min(nl, sp);
                if (nx == std::string::npos) break;
                cut = nx + 1;
                f->setText(str.substr(cut));
                f->layout();
            }
        }
    }
    if (!hx::isNull(endTimer)) {
        endTimer -= mt::Timer::tmod;
        if (endTimer <= 0) {
            endTimer = hx::NUL;
            quit();
        }
    }
}

// ------------------------------------------------------------------ Furi

void Furi::init() {
    People::init();
    seed = mt::OldRandom(Cs::pi->x * 10000 + Cs::pi->y);
    std::string str;
    str += pick("FURI_HELLO", seed);
    str += pick("FURI_ARGUE", seed);
    if (Cs::pi->gotItem(MissionInfo::EVASION)) {
        if (seed.random(2) == 0) {
            rewardMin = 20 + seed.random(180);
            str += Text::str("FURI_REWARD_MIN");
        } else {
            rewardCaps = 2 + seed.random(6);
            str += Text::str("FURI_REWARD_CAPS");
        }
        addBut(0, [this] { accept(); });
    } else {
        std::string end = pick("FURI_END_1", seed);
        str += Text::str("FURI_END_0") + end;
    }
    str = replaceAll(str, ":::rmin:", std::to_string(rewardMin));
    str = replaceAll(str, "::rmin::", std::to_string(rewardMin));
    str = replaceAll(str, "::rcap::", std::to_string(rewardCaps));
    setText(str);
    addBut(2, [this] { quit(); });
    if (Cs::pi->missions.get(35) == 0) addBut(3, [this] { betray(); });
}

void Furi::betray() {
    Game* g = Game::me;
    if (g->hero && g->hero->currentHouse) g->hero->currentHouse->mark();
    g->incCaps(5);
    endTimer = 70;
    removeAllButs();
    setText(pick("FURI_BETRAY", seed));
}

void Furi::accept() {
    Game* g = Game::me;
    if (g->hero && g->hero->currentHouse) g->hero->currentHouse->mark();
    if (rewardMin >= 0) g->incMinerai(rewardMin);
    if (rewardCaps >= 0) g->incCaps(rewardCaps);
    endTimer = 70;
    removeAllButs();
    setText(pick("FURI_LUCK", seed));
}

// ------------------------------------------------------------------ Info

void Info::init() {
    People::init();
    seed = mt::OldRandom(Cs::pi->x * 10000 + Cs::pi->y);
    initText();
    addBut(2, [this] { quit(); });
}

void Info::initText() {
    auto at = [](int n) { return MissionInfo::item(n); };
    if (seed.random(24) == 0) {  // CRYSTALS
        auto o = at(MissionInfo::CRYSTAL_0 + seed.random(5));
        setText(replaceAll(Text::str("GOSSIP_CRYSTAL"), "::coord::", coord(o.x, o.y)));
        return;
    }
    if (seed.random(16) == 0) {  // CORES
        auto o = at(MissionInfo::ANTIMAT_0 + seed.random(4));
        std::string str = pick("GOSSIP_NOYAUX_0", seed);
        str += Text::str("GOSSIP_NOYAUX_1");
        setText(replaceAll(str, "::coord::", coord(o.x, o.y)));
        return;
    }
    if (seed.random(14) == 0) {  // TABLETS
        auto o = at(MissionInfo::SCROLL_0 + seed.random(8));
        setText(replaceAll(Text::str("GOSSIP_TABLET"), "::coord::", coord(o.x, o.y)));
        return;
    }
    if (seed.random(100) == 0) {
        setText(Text::str("GOSSIP_ASPHALT"));
        return;
    }
    if (seed.random(7) == 0) {
        setText(pick("GOSSIP_DEFAULT", seed));
        return;
    }
    auto o = at(MissionInfo::MISSILE + seed.random(MissionInfo::MISSILE_MAX));
    std::string str = replaceAll(Text::str("GOSSIP_MISSILE_0"), "::coord::", coord(o.x, o.y));
    str += pick("GOSSIP_MISSILE_1", seed);
    setText(str);
}

// ------------------------------------------------------------------ Traveler

void Traveler::init() {
    People::init();
    mt::OldRandom& b = *bs;
    int szid = Game::me->level->zid;
    int ezid = -1, ex = 0, ey = 0;
    if (b.random(3) == 0) {  // a point in space
        ex = b.random(300) - 150;
        ey = b.random(300) - 150;
    } else {  // a planet
        for (int guard = 0; guard < 1000; guard++) {
            ezid = b.random(19);
            if (Game::isAlive(ezid) && ezid != szid && ezid < (int)ZoneInfo::list.size()) {
                ex = ZoneInfo::list[ezid].pos[0];
                ey = ZoneInfo::list[ezid].pos[1];
                break;
            }
        }
    }
    int dx = ex - Cs::pi->x, dy = ey - Cs::pi->y;
    int dist = std::max(std::abs(dx), std::abs(dy));
    if (b.random(10) > 0) rewardMin = hx::Int(dist * (5 + b.random(15)));
    if (b.random(3) == 0) rewardCaps = hx::Int(dist * b.rand() * 0.15);
    mission.sx = Cs::pi->x;
    mission.sy = Cs::pi->y;
    mission.ex = ex;
    mission.ey = ey;
    mission.start = szid;
    mission.dest = ezid;
    setText(getIntroText());
    if (Cs::pi->getLife() > 0) {
        addBut(0, [this] { accept(); });
        addBut(1, [this] { quit(); });
    } else {
        addBut(2, [this] { quit(); });
    }
}

void Traveler::accept() {
    Game* g = Game::me;
    lander::House* h = g->hero ? g->hero->currentHouse : nullptr;
    if (h) {
        h->mark();
        h->alienBehaviour = 0;
    }
    g->travel = mission;
    if (rewardMin >= 0) g->incMinerai(rewardMin);
    if (rewardCaps >= 0) g->incCaps(rewardCaps);
    quit();
}

std::string Traveler::getIntroText() {
    // Traveler.getInfo: who they are, from the destination
    mt::OldRandom s(mission.ex * 10000 + mission.ey);
    std::string name = pick("TRAVELER_NAMES", s);
    std::string profession = pick("TRAVELER_JOBS", s);
    std::string user = pick("TRAVELER_USER", s);
    std::string stuff0 = pick("TRAVELER_STUFF_0", s);
    std::string stuff1 = pick("TRAVELER_STUFF_1", s);
    std::string miss = pick("TRAVELER_MISS", s);
    std::string singer = pick("TRAVELER_SINGER", s);
    mission.name = name;

    std::string str;
    str += pick("TRAVELER_INTRO", s);
    str += pick("TRAVELER_WHO", s);
    nlohmann::json leave = nlohmann::json::array();
    for (auto& v : Text::raw("TRAVELER_LEAVE")) leave.push_back(v);
    const auto& lp = Text::raw("TRAVELER_LEAVE_PLANET");
    if (lp.is_array() && mission.start >= 0 && mission.start < (int)lp.size())
        for (auto& v : lp[mission.start]) leave.push_back(v);
    str += " " + pickJson(leave, s);
    if (mission.dest >= 0) {
        str += " " + pick("TRAVELER_DEST", s);
        if (s.random(2) == 0) {
            const auto& dp = Text::raw("TRAVELER_DEST_PLANET");
            if (dp.is_array() && mission.dest < (int)dp.size()) str += " " + pickJson(dp[mission.dest], s);
        }
    } else {
        str += " " + pick("TRAVELER_DEST_COORD", s);
    }
    if (Cs::pi->getLife() > 0) {
        str += "\n" + pick("TRAVELER_ASK_0", s);
        str += " " + pick("TRAVELER_ASK_1", s);
        if (rewardMin >= 0) {
            str += " " + pick("TRAVELER_REWARD_MIN_0", s);
            if (s.random(3) == 0) str += " " + pick("TRAVELER_REWARD_MIN_1", s);
        } else {
            str += " " + pick("TRAVELER_REWARD_KEUD", s);
        }
        if (rewardCaps >= 0) str += " " + pick("TRAVELER_REWARD_CAPS", s);
    } else {
        str += Text::str("TRAVELER_NO_SLOT");
    }
    auto zoneName = [](int id) {
        return id >= 0 && id < (int)ZoneInfo::list.size() ? ZoneInfo::list[id].name : std::string();
    };
    str = replaceAll(str, "::name::", name);
    str = replaceAll(str, "::profession::", profession);
    str = replaceAll(str, "::miss::", miss);
    str = replaceAll(str, "::user::", user);
    str = replaceAll(str, "::start::", zoneName(mission.start));
    str = replaceAll(str, "::end::", zoneName(mission.dest));
    str = replaceAll(str, "::pos::", coord(mission.ex, mission.ey));
    str = replaceAll(str, "::stuff0::", stuff0);
    str = replaceAll(str, "::stuff1::", stuff1);
    str = replaceAll(str, "::singer::", singer);
    str = replaceAll(str, "::rmin::", std::to_string(std::max(0, rewardMin)));
    str = replaceAll(str, "::rcap::", std::to_string(std::max(0, rewardCaps)));
    return str;
}

// ------------------------------------------------------------------ ItemGiver

lander::Scenario ItemGiver::getScn(int n) {
    int st = Cs::pi->items.get(n);
    if (st == SparseInts::NONE) {
        if (n == MissionInfo::KARBONITE) return lander::Scenario::ItemGiver;
        if (n >= MissionInfo::EMAP_0 && n <= MissionInfo::EMAP_0 + 41) return lander::Scenario::ItemGiver;
        return lander::Scenario::Locked;
    }
    if (st == MissionInfo::GROUND) return lander::Scenario::ItemGiver;
    return lander::Scenario::Empty;
}

void ItemGiver::init() {
    People::init();
    bool flOk = Cs::pi->items.get(itemId) == MissionInfo::GROUND;
    switch (itemId) {
        case MissionInfo::SALMEEN_COUSIN: setText(Text::str("ITEM_GIVER_SALMEEN_COUSIN")); break;
        case MissionInfo::BADGE_FURI: setText(Text::str("ITEM_GIVER_BADGE_FURI")); break;
        case MissionInfo::BALL_DOUBLE: setText(Text::str("ITEM_GIVER_SAUMIR")); break;
        case MissionInfo::RETROFUSER: setText(Text::str("ITEM_GIVER_SACTUS")); break;
        case MissionInfo::KARBONITE:
            setText(Text::str(flOk ? "ITEM_GIVER_SAFORI_1" : "ITEM_GIVER_SAFORI_0"));
            break;
        case MissionInfo::COMBINAISON: setText(Text::str("ITEM_GIVER_COMBINAISON")); break;
        default:
            if (itemId >= MissionInfo::TBL_0 && itemId <= MissionInfo::TBL_0 + 11) {
                std::string str = Text::str("ITEM_GIVER_TABLET_KARBONIS_0") +
                                  Text::at("ITEM_GIVER_TABLET_KARBONIS_1", itemId - MissionInfo::TBL_0) +
                                  Text::str("ITEM_GIVER_TABLET_KARBONIS_2");
                setText(str);
            } else if (itemId >= MissionInfo::EMAP_0 && itemId <= MissionInfo::EMAP_0 + 41) {
                mt::Rand r(itemId);
                price = 500 + r.random(4000);
                setText(replaceAll(Text::str("ITEM_GIVER_EMAP_0"), "::price::", std::to_string(price)));
            } else {
                flOk = false;
                setText(Text::str("ITEM_GIVER_TABLET_KARBONIS_3"));
            }
    }
    if (price >= 0) {
        if (price <= Cs::pi->minerai) addBut(0, [this] { accept(); });
        addBut(2, [this] { quit(); });
        return;
    }
    if (flOk) addBut(0, [this] { accept(); });
    else addBut(2, [this] { quit(); });
}

void ItemGiver::accept() {
    Game* g = Game::me;
    g->item = itemId;
    if (price >= 0) g->debit += price;
    if (g->hero && g->hero->currentHouse) g->hero->currentHouse->scn = lander::Scenario::Empty;
    Map::me->removeMenu(0);
    quit();
}

}  // namespace navi
