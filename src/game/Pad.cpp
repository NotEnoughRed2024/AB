// PadSkin.hx, Pad.hx
#include <cmath>

#include "Elements.h"
#include "Shell.h"

namespace Col = mt::Col;
namespace Filt = mt::Filt;
namespace Num = mt::Num;

// ------------------------------------------------------------------ PadSkin

PadSkin::PadSkin(MC mc) : Phys(std::move(mc)) {
    dm = std::make_unique<mt::DepthManager>(root);
    skinId = 0;
    if (Cs::pi->gotItem(MissionInfo::MEDAL)) skinId = 1;
}

MC PadSkin::part(const char* n) const { return root->clip(n); }

void PadSkin::setType(int n) {
    type = n;
    for (const char* p : {"mid", "side0", "side1"})
        if (auto c = part(p)) {
            c->gotoAndStop(type + 1);
            if (auto s = c->smc()) s->gotoAndStop(skinId + 1);
        }
}

void PadSkin::setRay(double r) {
    ray = r;
    double w = (r - SIDE) + 0.2;
    if (auto m = part("mid")) {
        m->xscale = w * 2;
        m->x = -w;
    }
    if (auto s = part("side0")) s->x = -r;
    if (auto s = part("side1")) s->x = r;
}

void PadSkin::setReactor(bool fl) {
    if (fl) {
        mcReactor = dm->attach("mcReactor", 0);
        if (auto s = mcReactor->smc()) s->visible = true;
        mcReactor->y = 10.5;
    } else {
        if (mcReactor) mcReactor->removeMovieClip();
        mcReactor = nullptr;
    }
}

void PadSkin::explode(MC bmc) {
    mt::DepthManager pdm(bmc);
    auto* sp = new Phys(bmc);
    sp->x = x;
    sp->y = y;
    sp->updatePos();
    sp->timer = 100;
    sp->root->rotation = root->rotation;
    int max = hx::Int(2 * ray / Cs::BW);
    double inc = std::fmod(2 * ray, max) / max;
    for (int i = 0; i < max; i++) {
        MC mc = pdm.attach("partExplode", 0);
        double ppx = -ray + i * (Cs::BW + inc);
        mc->x = ppx;
        mc->y = 0;
        Col::setColor(mc.get(), 0xFCF0B0, -220);
        for (int n = 0; n < 10; n++) {
            auto* p = new Phys(pdm.attach("mcPart", 0));
            p->root->gotoAndStop(hx::random(p->root->totalFrames()));
            p->x = ppx + hx::random() * (Cs::BW + inc);
            p->y = hx::random() * 12;
            p->vy = 0.75 - hx::random() * 1.5;
            p->vx = (p->x / ray) * 3;
            Col::setColor(p->root.get(), 0xFCF0B0, -(200 + hx::random(50)));
            p->timer = 10 + hx::random() * 20;
            p->fadeType = 0;
            p->setScale(50 + hx::random() * 70);
            p->root->rotation = hx::random(360);
            p->updatePos();
        }
    }
    MC mc = pdm.attach("fxPadExplosion", 0);
    mc->y = HEIGHT * 0.5;
    if (auto s = mc->smc())
        if (auto ss = s->smc()) {
            ss->xscale = ray * 2;
            ss->blendMode = flash::Blend::Add;
        }
    for (int i = 0; i < 4; i++) {
        MC m = pdm.attach("fxPadMiniExplosion", Game::DP_PARTS);
        m->x = (hx::random() * 2 - 1) * ray * 0.8;
        m->y = HEIGHT * 0.5 + (hx::random() * 2 - 1) * 2;
        m->xscale = m->yscale = 100 + hx::random() * 100;
        m->gotoAndPlay(hx::random(4) + 1);
        m->blendMode = flash::Blend::Add;
    }
    kill();
}

// ------------------------------------------------------------------ Pad

static const int X_KEY = 'X';

Pad::Pad(MC mc) : PadSkin(std::move(mc)) {
    ty = Cs::getY(Cs::YMAX + DY);
    x = Cs::mcw * 0.5;
    y = ty + 50;
    freeze = 0;
    glueCount = 0;
    insect = 0;
    flMouse = false;
    flGuardian = false;
    int max = 1;
    if (Cs::pi->gotItem(MissionInfo::BALL_DOUBLE)) max++;
    // note: Game.pad is only assigned after this constructor returns (it may be null below)
    for (int i = 0; i < max; i++) initStartBall();
    Game::me->flFirstBall = true;
}

el::Ball* Pad::initStartBall() {
    el::Ball* b = Game::me->newBall();
    double rnd = (hx::random() * 2 - 1) * 0.5;
    if (Game::PLAY_AUTO) rnd = 0.5;
    b->gluePoint = rnd * 20;
    b->moveTo(x, y);
    b->vx = 0;
    b->vy = 1;
    b->update();
    b->colPad(rnd);
    b->flReady = false;
    return b;
}

void Pad::init() {
    flStop = false;
    power = hx::NUL;
    moveFactor = 0.5;
    if (skinId == 1) moveFactor = 1;
    if (Game::me->level->zid == ZoneInfo::SPIGNYSOS) {
        moveFactor *= 0.1;
        flIce = true;
    }
    setRay(Cs::pi->getRay());
    setType(Cs::PAD_STANDARD);
}

void Pad::update() {
    moveHeight();
    if (!checkMissiles() && !flGuardian) updatePlay();
    updateFlash();
    if (!hx::isNull(chargeTimer)) updateCharge();
    Phys::update();
}

void Pad::updatePlay() {
    move();
    if (Game::me->flPress) salve();
    if (type == Cs::PAD_SHAKE) {
        x += (hx::random() * 2 - 1) * 14;
        queue("mcPinkBar", 15);
    }
    if (!hx::isNull(power)) updatePower();
}

void Pad::move() {
    if (freeze > 0) {
        x += (hx::random() * 2 - 1) * 2;
        return;
    }
    double dx = 0;
    auto* st = flash::Stage::get();
    if (Game::PLAY_AUTO) {
        if (hx::isNull(padec)) {
            newPadec();
            padec = tpadec;
        }
        padec += (tpadec - padec) * 0.1;
        el::Ball* b = Game::me->getLowestBall();
        dx = b ? (b->x + padec * ray) - x : 0;
        if (!b || !(b->x > 0)) dx = 0;
        double dist = std::min(std::fabs(dx), 40.0);
        double sens = dx == 0 ? 1 : std::fabs(dx) / dx;
        dx = sens * dist;
    } else {
        if (Cs::PREF_BOOLS[0]) {
            dx = 0;
            double speed = 20;
            if (st->isDown(37)) dx = -speed;
            if (st->isDown(39)) dx = speed;
            if (st->isDown(16)) dx *= 2;
            if (moveFactor == 1) dx *= 0.9;
            if (Game::me->flPress) {
                if (!st->isDown(17)) Game::me->mouseUp();
            } else {
                if (st->isDown(17)) Game::me->mouseDown();
            }
        } else {
            dx = getPadX() - x;
        }
    }
    x += dx * moveFactor;
    x += (hx::random() * 2 - 1) * insect * 4;
    if (flIce || glueCount > 0) {
        int max = hx::Int(std::min(std::fabs(dx * 0.02), 5.0));
        for (int i = 0; i < max; i++) {
            std::string link = "partIce";
            double sc = 100.0;
            if (glueCount > 0) {
                link = "mcGlue";
                sc = 10 + hx::random() * 20;
            }
            auto* p = new Phys(Game::me->dm->attach(link, Game::DP_PARTS));
            p->x = x + (hx::random() * 2 - 1) * ray;
            p->y = y + hx::random() * 10;
            p->vr = (hx::random() * 2 - 1) * 10;
            p->root->rotation = hx::random() * 360;
            p->fadeType = 0;
            p->timer = 10 + hx::random() * 10;
            p->weight = 0.1 + hx::random() * 0.1;
            p->vx = dx * hx::random() * 0.02;
            p->vy = -hx::random() * std::fabs(dx) * 0.02;
            p->setScale(sc);
        }
    }
}

void Pad::moveHeight() {
    if (!hx::isNull(ty)) {
        double dy = ty - y;
        y += dy * 0.2 * mt::Timer::tmod;
    }
}

void Pad::newPadec() { tpadec = (hx::random() * 2 - 1) * 0.5; }

void Pad::initPower() {
    power = 1;
    displayPowerBar();
    if (auto m = part("mid"))
        if (auto b = m->clip("bar")) b->alpha = 100;
}

void Pad::updatePower() {
    if (power < 1) {
        power = std::min(power + recovery * mt::Timer::tmod, 1.0);
        displayPowerBar();
        if (power == 1)
            if (auto m = part("mid"))
                if (auto b = m->clip("bar")) b->alpha = 100;
    }
}

void Pad::displayPowerBar() {
    if (auto m = part("mid"))
        if (auto b = m->clip("bar")) b->xscale = 100 * power;
}

void Pad::action() {
    if (!hx::isNull(chargeTimer)) launchCharge();
    switch (type) {
        case Cs::PAD_TIME:
            if (power > 0.2) flTransform = true;
            break;
        case Cs::PAD_LASER: {
            double cost = 0.2;
            if (power > cost) {
                power -= cost;
                for (int i = 0; i < 2; i++) {
                    auto* shot = new el::shot::Laser(Game::me->dm->attach("mcLaser", Game::DP_PARTS));
                    shot->moveTo(x + (i * 2 - 1) * (ray - 9), y);
                    shot->setVit(18);
                    shot->updatePos();
                }
            }
            break;
        }
        case Cs::PAD_GENERATOR:
            if (power == 1) {
                if (auto m = part("mid"))
                    if (auto b = m->clip("bar")) b->alpha = 60;
                power = 0;
                initStartBall();
                Game::me->flPress = false;
                setFlash();
            }
            break;
    }
}

void Pad::release() {
    if (type == Cs::PAD_TIME) {
        trg = nullptr;
        flTransform = false;
    }
}

void Pad::salve() {
    Game* g = Game::me;
    if (y < Cs::mch)
        for (auto* b : g->balls) b->unglue();
    switch (type) {
        case Cs::PAD_AIMANT: {
            double dec = 0.04;
            if (power > dec * 2) {
                power = std::max(power - dec, 0.0);
                for (auto* b : g->balls) {
                    if (b->type != Cs::BALL_KAMIKAZE && b->type != Cs::BALL_SHADE && b->y < y) {
                        double a = std::atan2(b->vy, b->vx);
                        if (hx::isNull(b->pdec)) b->pdec = (hx::random() * 2 - 1) * 0.5;
                        double dx = x + b->pdec * ray - b->x;
                        double dy = y - b->y;
                        double ta = std::atan2(dy, dx);
                        double dist = std::sqrt(dx * dx + dy * dy);
                        double cca = Num::mm(0.25, (b->speed / 5) * 0.25, 0.5);
                        a += Num::hMod(ta - a, 3.14) * cca;
                        b->vx = std::cos(a) * b->speed;
                        b->vy = std::sin(a) * b->speed;
                        if (dist < 250 && hx::random() * dist < 30) {
                            auto* p = new fx::Attract(g->dm->attach("partLine", Game::DP_PARTS));
                            double ec = 12;
                            p->x = b->x + (hx::random() * 2 - 1) * ec;
                            p->y = b->y + (hx::random() * 2 - 1) * ec;
                            p->dx = (hx::random() * 2 - 1) * ray;
                            Filt::glow(p->root.get(), 8, 2, 0xFFFFFF);
                        }
                    }
                }
            }
            break;
        }
        case Cs::PAD_TIME:
            if (flTransform && power > 0) {
                if (!trg) trg = findTransformTrg();
                if (trg) {
                    power = std::max(power - 0.04, 0.0);
                    trg->incTransform(0.035, 0);
                    trg->fxFrout(2);
                    MC mc = g->dm->empty(0);
                    mt::DepthManager tdm(mc);
                    MC mmc = tdm.empty(0);
                    mt::DepthManager dm2(mmc);
                    mmc->lineStyle(1, 0xFFFFFF, 100);
                    for (int i = 0; i < 2; i++) {
                        int sens = i * 2 - 1;
                        double m = 0.15;
                        double bx = Cs::getX(trg->x + m + (hx::random() * (1 - 2 * m)));
                        double by = Cs::getY(trg->y + m + (hx::random() * (1 - 2 * m)));
                        mmc->moveTo(x + (ray - 4) * sens, y);
                        mmc->lineTo(bx, by);
                        MC mmmc = dm2.attach("mcTransformerImpact", 0);
                        mmmc->x = bx;
                        mmmc->y = by;
                    }
                    Filt::glow(mmc.get(), 10, 2, 0xFF8800);
                    g->plasmaDraw(mc.get());
                    mc->removeMovieClip();
                    if (trg->type == 0 || trg->flDeath) trg = nullptr;
                    displayPowerBar();
                    if (power == 0) release();
                }
            }
            break;
    }
}

Block* Pad::findTransformTrg() {
    for (int n = 0; n < 3; n++)
        for (int k = 0; k < 2; k++) {
            int sens = k * 2 - 1;
            int ppx = Cs::getPX(x) + sens * n;
            for (int i = 0; i < Cs::YMAX; i++) {
                int ppy = Cs::YMAX - (i + 1);
                Block* bl = Game::me->gridAt(ppx, ppy);
                if (bl && bl->type != 0) return bl;
            }
        }
    return nullptr;
}

void Pad::setType(int n) {
    switch (type) {
        case Cs::PAD_GLUE:
            for (auto* b : Game::me->balls) b->unglue();
            break;
        case Cs::PAD_AIMANT:
            flAttract = false;
            break;
    }
    PadSkin::setType(n);
    switch (type) {
        case Cs::PAD_TIME:
            recovery = 0.01;
            initPower();
            break;
        case Cs::PAD_LASER:
            recovery = 0.007;
            initPower();
            break;
        case Cs::PAD_GENERATOR:
            recovery = 0.005;
            initPower();
            break;
        case Cs::PAD_AIMANT:
            recovery = 0.005;
            initPower();
            break;
    }
}

void Pad::bounceBall(el::Ball* ball) {
    checkDrone(ball->x, ball->y);
    y += std::min(ball->speed * 0.5, 10.0);
    updatePos();
}

void Pad::initCharge() {
    chargeTimer = 0;
    if (!mcCharge) {
        mcCharge = mt::DepthManager(root).attach("fxFlurch", 2);
        mcCharge->x = 0;
        mcCharge->xscale = ray * 2;
        mcCharge->yscale = 0;
    }
}

void Pad::updateCharge() {
    Game* g = Game::me;
    chargeTimer += mt::Timer::tmod;
    flh = std::min((chargeTimer / 40), 1.0);
    mcCharge->yscale = chargeTimer * 0.5;
    double ma = 30;
    for (int i = 0; i < 2; i++) {
        auto* p = new fx::Attract(g->dm->attach("partLine", Game::DP_PARTS));
        double cx = (hx::random() * 2 - 1);
        double cy = hx::random();
        if (i == 1) cx *= 0.25;
        p->x = x + cx * (ray + ma);
        p->y = y + cy * (10 + 2 * ma) - ma;
        p->dx = cx * ray;
        p->dy = cy * 10;
        Filt::glow(p->root.get(), 8, 2, 0xFFFFFF);
        if (i == 1) p->y -= (1 - cx * 4) * chargeTimer;
    }
    if (chargeTimer > 120) {
        launchCharge();
        return;
    }
    int ppx = Cs::getPX(x);
    if (ppx != opx) unSelectBlocks();
    for (int ppy = 0; ppy < Cs::YMAX; ppy++) {
        Block* bl = g->gridAt(ppx, ppy);
        if (bl) Col::setColor(bl->root.get(), 0, 50);
        // the original pushes nulls too; they're skipped when unselecting
        if (bl) selectedBlocks.push_back(bl);
    }
    opx = ppx;
}

void Pad::launchCharge() {
    unSelectBlocks();
    new ev::Javelot();
    chargeTimer = hx::NUL;
    if (mcCharge) mcCharge->removeMovieClip();
    mcCharge = nullptr;
}

void Pad::unSelectBlocks() {
    while (!selectedBlocks.empty()) {
        Col::setColor(selectedBlocks.back()->root.get(), 0, 0);
        selectedBlocks.pop_back();
    }
    opx = -1000;
}

void Pad::launchMissile() {
    Game* g = Game::me;
    flAutoMissile = false;
    fxExplosion(x - 5, y - (ray + 5));
    auto* shot = new el::shot::Missile(g->dm->attach("mcMissile", Game::DP_PARTS));
    shot->moveTo(x - 5, y - ray);
    shot->root->rotation = -90;
    shot->setType(g->missileType);
    shot->updatePos();
    shot->vy = -5;
    y += 40;
    ty = Cs::getY(Cs::YMAX + DY);
    g->incMissile(-1);
}

bool Pad::checkMissiles() {
    Game* g = Game::me;
    bool flTest = false;
    if (Game::PLAY_AUTO && g->balls.size() < 3) {
        bool flOk = false;
        int px = Cs::getPX(x);
        for (int i = 0; i < g->level->ymax; i++) {
            int py = g->level->ymax - i;
            Block* bl = g->gridAt(px, py);
            if (bl) {
                flOk = Block::isSoft(bl->type);
                break;
            }
        }
        if (flOk) {
            el::Ball* lb = g->getLowestBall();
            if (lb && (lb->y < Cs::mch - 100 || lb->vy < 0)) {
                if (flAutoMissile) flTest = true;
                else if (Cs::pi->missile > 0)
                    flAutoMissile = hx::random(hx::Int(200.0 * g->balls.size() / Cs::pi->missile)) == 0;
            }
        }
    } else {
        auto* st = flash::Stage::get();
        flTest = st->isDown(flash::Key::SPACE) || st->isDown(X_KEY);
    }
    if (flTest && g->missile > 0) {
        if (hx::isNull(missileCoef)) missileCoef = 0;
        missileCoef = std::min(missileCoef + g->missileTurnSpeed * mt::Timer::tmod, 1.0);
    } else {
        if (!hx::isNull(missileCoef)) {
            missileCoef = std::max(0.0, missileCoef - g->missileTurnSpeed * mt::Timer::tmod);
            if (missileCoef == 0) {
                root->rotation = 0;
                missileCoef = hx::NUL;
                missileCooldown = hx::NUL;
            }
        }
    }
    if (!hx::isNull(missileCoef)) {
        for (auto* b : g->balls) b->unglue();
        root->rotation = 90 * missileCoef;
        if (missileCoef == 1) {
            if (hx::isNull(missileCooldown)) {
                missileCooldown = 5;
                if (missileCoef > 0.5) {
                    for (Block* bl : g->blocks)
                        if (bl->type == Block::GUARDIAN) {
                            bl->explode();
                            flGuardian = true;
                            break;
                        }
                }
            }
            missileCooldown -= mt::Timer::tmod;
            if (missileCooldown <= 0) {
                missileCooldown += g->missileCadence;
                launchMissile();
            }
        }
        return true;
    }
    return false;
}

void Pad::checkDrone(double sx, double sy) {
    if ((int)drones.size() < Cs::pi->drone && ev::Drone::getBlock()) {
        auto* drone = new ev::Drone();
        drone->root->x = sx;
        drone->root->y = sy;
        drones.push_back(drone);
        drone->sparkTimer = 10;
    }
}

void Pad::explode(MC mc) {
    Game::me->setFlash(1, -0.15);
    unSelectBlocks();
    while (!drones.empty()) {
        auto* d = drones.back();
        drones.pop_back();
        d->explode();
    }
    PadSkin::explode(mc);
}

void Pad::fxExplosion(double fx_, double fy) {
    Game* g = Game::me;
    double eray = 70;
    double pq = 1;
    double inc = -10;
    double mult = 0.8;
    flash::ColorTransform ct(0.95, mult, mult, 1, inc, inc * 2, inc * 2, -10);
    double blp = std::max(10 * pq * mt::Timer::tmod, 1.0);
    auto* sp = new Phys(g->dm->empty(Game::DP_PARTS));
    sp->x = fx_ - eray;
    sp->y = fy - eray;
    sp->updatePos();
    sp->timer = 20;
    mt::DepthManager edm(sp->root);
    auto* pl = new mt::Plasma(edm.empty(0), hx::Int(eray * 2), hx::Int(eray * 2), 1);
    pl->ct = std::make_unique<flash::ColorTransform>(ct);
    pl->filters = {flash::Filter::blur(blp, blp)};
    int max = 4;
    for (int i = 0; i < max; i++) {
        double a = ((double)i / max) * 6.28 + (hx::random() * 2 - 1) * 0.3;
        double ca = std::cos(a), sa = std::sin(a);
        double speed = 0.5 + hx::random() * 3;
        double cr = 5;
        auto* p = new mt::Part(edm.attach("mcExploPart", 0));
        p->x = eray + ca * speed * cr;
        p->y = eray + sa * speed * cr;
        p->vx = ca * speed;
        p->vy = sa * speed - 2;
        p->fadeType = 0;
        p->timer = 10 + hx::random() * 10;
        p->root->rotation = hx::random() * 360;
        p->bmp = pl;
        p->updatePos();
        p->setScale(50 + hx::random() * 50);
    }
    MC mc = g->dm->attach("mcExplo", 0);
    for (int i = 0; i < 3; i++) {
        mc->x = eray + (hx::random() * 2 - 1) * 10;
        mc->y = eray + (hx::random() * 2 - 1) * 10;
        mc->xscale = mc->yscale = eray * 2;
        mc->rotation = hx::random() * 360;
        // "mc.blendMode.add = true" in the original is a no-op on a string
        pl->drawMc(mc.get());
    }
    mc->removeMovieClip();
}

void Pad::powerUp() {
    int max = hx::Int(18 * Cs::PREF_GFX);
    for (int i = 0; i < max; i++) {
        auto* p = new fx::LineUp(Game::me->dm->attach("partLineUp", Game::DP_PARTS));
        p->x = x + (hx::random() * 2 - 1) * ray;
        p->y = y;
        p->sleep = hx::random() * 5;
        p->timer = 10 + hx::random() * 20;
        p->weight = -(0.1 + hx::random() * 0.3);
        p->root->blendMode = flash::Blend::Add;
        p->factor = 3;
        Filt::glow(p->root.get(), 10, 2, 0xFFFFFF);
    }
}

void Pad::setFlash(double n) {
    flh = n;
    updateFlash();
}

void Pad::updateFlash() {
    double c = std::min(flh, 1.0);
    flh *= 0.7;
    if (flh < 0.1) c = 0;  // "flh == null" in the original is a comparison, so flh keeps decaying
    Col::setColor(root.get(), 0, hx::Int(c * 255));
    root->filters.clear();
    if (c > 0) Filt::glow(root.get(), 16 * c, 2 * c, 0xFFFFFF);
}

void Pad::queue(const std::string& link, double alpha) {
    MC brush = Game::me->dm->attach(link, 0);
    brush->setHeight(12);
    brush->setWidth(ray * 2);
    brush->x = x - ray;
    brush->y = y;
    brush->alpha = alpha;
    Game::me->plasmaDraw(brush.get());
    brush->removeMovieClip();
}

double Pad::getPadX() {
    double c = (((double)Cs::MX / Cs::mcw) * 2 - 1) * (1 + Cs::PREF_MOUSE);
    return Cs::mcw * (0.5 + c * 0.5);
}
