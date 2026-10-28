// el/Ball.hx, el/Molecule.hx, el/Shot.hx, el/shot/Laser.hx, el/shot/Missile.hx
#include <cmath>

#include "Elements.h"

namespace Col = mt::Col;
namespace Filt = mt::Filt;
namespace Num = mt::Num;

namespace el {

// ------------------------------------------------------------------ Ball

Ball::Ball(MC mc) : Element(std::move(mc)) {
    Game::me->balls.push_back(this);
    ox = 0;
    oy = 0;
    px = hx::Int(Cs::XMAX * 0.5);
    py = hx::Int(Cs::YMAX - 3);
    vx = (hx::random() * 2 - 1);
    vy = -(4 + hx::random() * 2);
    speed = 5;
    flUp = true;
    flImmortal = false;
    damage = 1;
    flReady = true;
    initFamily();
    setType(Cs::BALL_STANDARD);
    if (Cs::PREF_BOOLS[3]) Filt::glow(root.get(), 2, 2, 0);
}

void Ball::initFamily() {
    fam = 0;
    if (Cs::pi->gotItem(MissionInfo::BALL_DRILL)) fam = 1;
    if (Cs::pi->gotItem(MissionInfo::BALL_SOLDAT)) fam = 2;
    if (Cs::pi->gotItem(MissionInfo::BALL_POWER)) fam = 3;
    if (Cs::pi->gotItem(MissionInfo::BALL_BLACK)) fam = 4;
    if (fam >= 3) damage += 1;
    if (fam >= 4) {
        damage += 1;
        flPierce = true;
        pierceDamage = damage;
    }
    baseDamage = damage;
}

void Ball::update() {
    if (flGhost) {  // the original swaps update() for updateGhost() while ghosted
        updateGhost();
        return;
    }
    Game* g = Game::me;
    flBounce = false;
    if (!hx::isNull(gluePoint)) {
        if (!g->pad) {
            // Game.pad is null while a new pad is being built: Flash computes NaN positions here
            if (!flReady) flReady = true;
            return;
        }
        double nx = g->pad->x + gluePoint;
        if (nx < ray || nx > Cs::mcw - ray) {
            if (flReady) gluePoint = hx::NUL;
        } else {
            if (!flReady) flReady = true;
            y = g->pad->y - ray;
            x = nx;
            moveTo(x, y);
            updatePos();
            if (type == Cs::BALL_FIRE)
                if (auto s = root->smc()) s->rotation = 90;
            return;
        }
    } else {
        if (g->flFirstBall) g->flFirstBall = false;
    }
    if (!hx::isNull(sleep)) {
        sleep -= mt::Timer::tmod;
        if (sleep < 0) sleep = hx::NUL;
        return;
    }
    Element::update();
    checkPad();
    if (!flUp && y > Cs::mch + 10 && !flImmortal) {
        if (g->balls.size() == 1 && g->levelTimer < 600 && g->flSafe && type != Cs::BALL_SHADE) {
            moveTo(x, Cs::mch + 10);
            vy *= -1;
            flUp = true;
            g->newTitle("SAUVETAGE !", 0xFF0000, true);
        } else {
            destroy();
        }
    }
    switch (type) {
        case Cs::BALL_FIRE: {
            double a = std::atan2(vy, vx);
            if (auto s = root->smc()) s->rotation = a / 0.0174;
            genSparks(1, 20);
            break;
        }
        case Cs::BALL_ICE:
            genSparks(2, 10);
            genIceShards();
            g->plasmaDraw(root.get());
            break;
        case Cs::BALL_VOLT:
            if (mt::Timer::tmod < 1.5 && hx::random(5) == 0) {
                MC mc = g->dm->attach("mcBolt", Game::DP_PARTS);
                mc->xscale = mc->yscale = 50 + hx::random() * 100;
                Filt::glow(mc.get(), 10, 1, 0xFFFF00);
                mc->blendMode = flash::Blend::Add;
                mc->x = x;
                mc->y = y;
                mc->rotation = hx::random() * 360;
            }
            break;
        case Cs::BALL_DRUNK: {
            double a = std::atan2(vy, vx);
            va += (hx::random() * 2 - 1) * 0.03 * (speed / 6) * mt::Timer::tmod;
            va *= std::pow(0.95, mt::Timer::tmod);
            a += va * mt::Timer::tmod;
            vx = std::cos(a) * speed;
            vy = std::sin(a) * speed;
            genBubbles();
            g->plasmaDraw(root.get());
            break;
        }
        case Cs::BALL_KAMIKAZE: {
            if (!trg || trg->flDeath || !Block::isSoft(trg->type)) {
                trg = g->blocks.empty() ? nullptr : g->blocks[hx::random((int)g->blocks.size())];
                ca = 0.01;
            }
            if (trg) {
                double a = std::atan2(vy, vx);
                double dx = Cs::getX(trg->x + 0.5) - x;
                double dy = Cs::getY(trg->y + 0.5) - y;
                double ta = std::atan2(dy, dx);
                ca = std::min(ca + 0.002 * mt::Timer::tmod, 1.0);
                va += Num::hMod(ta - a, 3.14) * ca;
                va *= std::pow(0.8, mt::Timer::tmod);
                a += va;
                vx = std::cos(a) * speed;
                vy = std::sin(a) * speed;
                if (hx::random() * 60 / mt::Timer::tmod < 1) trg = nullptr;
            }
            g->plasmaDraw(root.get());
            break;
        }
        case Cs::BALL_YOYO: {
            double sp = speed * 4 * (1 - (y / (Cs::mch + 15)));
            double a = std::atan2(vy, vx);
            vx = std::cos(a) * sp;
            vy = std::sin(a) * sp;
            g->plasmaDraw(root.get());
            break;
        }
        default:
            g->plasmaDraw(root.get());
    }
    if (flBounce) {
        for (int i = 0; i < 2; i++) {
            double a = std::atan2(vy, vx);
            double ba = i * 3.14;
            double da = Num::hMod(ba - a, 3.14);
            double la = 1.57 - ANGLE_MAX;
            if (std::fabs(da) < la) {
                double dif = la - std::fabs(da);
                double sens = da == 0 ? 1 : std::fabs(da) / da;
                a -= dif * sens;
                setAngle(a);
            }
        }
    }
}

void Ball::checkPad() {
    Pad* pad = Game::me->pad;
    if (!pad) {
        // comparisons with an undefined pad are all false in Flash
        return;
    }
    if (flUp && vy > 0 && y > pad->y - ray) {
        double cx = (x - pad->x) / pad->ray;
        if (std::fabs(cx) < 1 && hx::isNull(pad->missileCoef)) {
            if (type == Cs::BALL_SHADE) {
                destroy();
                return;
            }
            colPad(cx);
        } else {
            flUp = false;
        }
    }
}

void Ball::colPad(double cx) {
    Game* g = Game::me;
    Pad* pad = g->pad;
    if (pad) pad->bounceBall(this);
    pdec = hx::NUL;
    if (pad) {
        moveTo(x, pad->y - ray);
        updatePos();
    }
    double a = -1.57 + cx * ANGLE_MAX;
    vx = std::cos(a) * speed;
    vy = std::sin(a) * speed;
    if (pad) pad->newPadec();
    if (type == Cs::BALL_HALO) {
        setGhost(true);
    } else if (pad && pad->type == Cs::PAD_GLUE) {
        g->autoLaunchTimer = 0;
        gluePoint = x - pad->x;
        int max = 10;
        for (int i = 0; i < max; i++) {
            double aa = -(double)i / max * 3.14;
            double cca = std::cos(aa), ssa = std::sin(aa);
            double sp = 0.5 + hx::random() * 3;
            double cr = 4;
            auto* p = new Phys(g->dm->attach("partGlue", 0));
            p->x = x + cca * sp * cr;
            p->y = y + ssa * sp * cr;
            p->vx = cca * sp;
            p->vy = ssa * sp;
            p->weight = 0.1 + hx::random() * 0.15;
            p->timer = 10 + hx::random() * 10;
            p->setScale(p->weight * 400);
            p->fadeType = 0;
        }
    }
    if (flPierce) {
        if (auto s = root->smc())
            if (auto ss = s->smc()) ss->xscale = ss->yscale = 100;
        pierceDamage = damage;
    }
}

void Ball::setGhost(bool fl) {
    Element::setGhost(fl);
    if (!flGhost) Game::me->hit(px, py, this);
}

void Ball::setType(int n) {
    if (type != n && type != -1) fxPowerUp();
    if (type == Cs::BALL_SHADE) return;
    type = n;
    root->gotoAndStop(type + 1);
    damage = baseDamage;
    if (type == 0)
        if (auto s = root->smc()) s->gotoAndStop(1 + fam);
    switch (type) {
        case Cs::BALL_STANDARD: ray = 4; break;
        case Cs::BALL_FIRE:
            damage += 1;
            ray = 5;
            break;
        case Cs::BALL_VOLT:
            damage = 0.1;
            ray = 5;
            break;
        case Cs::BALL_ICE: ray = 4; break;
        case Cs::BALL_DRUNK:
            ray = 4;
            va = 0;
            break;
        case Cs::BALL_KAMIKAZE:
            ray = 5;
            va = 0;
            break;
        case Cs::BALL_HALO: ray = 4; break;
    }
}

void Ball::setSpeed(double n) {
    speed = n;
    double a = std::atan2(vy, vx);
    vx = std::cos(a) * speed;
    vy = std::sin(a) * speed;
}

void Ball::setAngle(double a) {
    angle = a;
    vx = std::cos(a) * speed;
    vy = std::sin(a) * speed;
}

void Ball::onBounce(int bpx, int bpy) {
    Game* g = Game::me;
    flBounce = true;
    if (flPierce) {
        if (pierceDamage > 0) {
            Block* bl = g->gridAt(bpx, bpy);
            if (bl && !hx::isNull(bl->life)) {
                pierceDamage -= bl->life;
                if (pierceDamage > 0 && type != Cs::BALL_ICE) flBounce = false;
            }
        } else {
            if (auto s = root->smc())
                if (auto ss = s->smc()) ss->xscale = ss->yscale = 60;
        }
    }
    g->hit(bpx, bpy, this);
    if (type == Cs::BALL_SHADE && bpx > 0 && bpx < Cs::XMAX) destroy();
    Element::onBounce(bpx, bpy);
    if (!flBounce) {
        if (this->px != bpx) vx *= -1;
        if (this->py != bpy) vy *= -1;
    }
}

void Ball::onEnterSquare(int, int) {
    Game* g = Game::me;
    auto* ms = g->monsters(px, py);
    Molecule* mon = (ms && !ms->empty()) ? (*ms)[0] : nullptr;
    if (mon) {
        flash::Point mp = mon->getPos(), bp = getPos();
        double dx = bp.x - mp.x, dy = bp.y - mp.y;
        double dist = std::sqrt(dx * dx + dy * dy);
        if (dist < mon->ray + 3) {
            setAngle(std::atan2(dy, dx));
            mon->damage(this);
        }
    }
    if (type == Cs::BALL_VOLT) {
        int r = 2;
        int max = r * 2 + 1;
        for (int dx = 0; dx < max; dx++)
            for (int dy = 0; dy < max; dy++) {
                Block* bl = g->gridAt(px + dx - r, py + dy - r);
                if (bl && !hx::isNull(bl->life)) {
                    bl->damage(this);
                    bl->fxBolt();
                }
            }
    }
}

void Ball::unglue() {
    if (x > Cs::SIDE && x < Cs::mcw - Cs::SIDE) gluePoint = hx::NUL;
}

Ball* Ball::clone() {
    Ball* ball = Game::me->newBall();
    ball->moveTo(x, y);
    ball->updatePos();
    ball->speed = speed;
    ball->setType(type);
    ball->vx = vx;
    ball->vy = vy;
    ball->gluePoint = gluePoint;
    return ball;
}

void Ball::genSparks(int fr, double turn) {
    if (hx::random((int)Sprite::spriteList.size()) < 20) {
        auto* p = new Phys(Game::me->dm->attach("partSpark", Game::DP_UNDERPARTS));
        p->x = x;
        p->y = y;
        double c = 0.3 + hx::random() * 0.5;
        p->vx = c * vx;
        p->vy = c * vy;
        p->vr = (hx::random() * 2 - 1) * turn;
        p->root->rotation = hx::random() * 360;
        p->timer = 10 + hx::random() * 30;
        p->root->gotoAndStop(fr);
        if (auto s = p->root->smc()) s->x = hx::random() * 15;
        p->frict = 0.95;
        p->root->blendMode = flash::Blend::Add;
    }
}

void Ball::genIceShards() {
    if (hx::random((int)Sprite::spriteList.size()) < 15) {
        auto* p = new Phys(Game::me->dm->attach("partIceShard", Game::DP_UNDERPARTS));
        p->x = x + (hx::random() * 2 - 1) * 4;
        p->y = y + (hx::random() * 2 - 1) * 4;
        double c = 0.7 + hx::random() * 0.3;
        p->vx = c * vx;
        p->vy = c * vy;
        p->vr = (hx::random() * 2 - 1) * 8;
        p->root->rotation = std::atan2(vy, vx) / 0.0174;
        p->timer = 10 + hx::random() * 10;
        p->fadeType = 0;
        p->weight = 0.1 + hx::random() * 0.1;
    }
}

void Ball::genBubbles() {
    if (hx::random((int)Sprite::spriteList.size()) < 15) {
        auto* p = new Phys(Game::me->dm->attach("partBubble", Game::DP_UNDERPARTS));
        p->x = x + (hx::random() * 2 - 1) * 4;
        p->y = y + (hx::random() * 2 - 1) * 4;
        double c = 0.1 + hx::random() * 0.2;
        p->vx = c * vx;
        p->vy = c * vy;
        p->timer = 10 + hx::random() * 20;
        p->fadeType = 0;
        p->weight = -(0.1 + hx::random() * 0.2);
        p->setScale(50 + hx::random() * 100);
    }
}

void Ball::fxLight() {
    int max = 24;
    mt::DepthManager bdm(root);
    for (int i = 0; i < max; i++) {
        double a = hx::random() * 6.28;
        auto* sp = new Phys(bdm.attach("mcUltraRay", 0));
        sp->root->rotation = a / 0.0174;
        sp->root->yscale = 5;
        sp->root->xscale = 50 + hx::random() * 50;
        sp->root->blendMode = flash::Blend::Add;
        sp->vr = hx::random() * 2 - 1;
        sp->updatePos();
        if (i > 8) {
            sp->sleep = hx::random() * 10;
            sp->root->visible = false;
            sp->root->stop();
        }
    }
}

void Ball::fxPowerUp() { mt::DepthManager(root).attach("fxBallPowerUp", 0); }

void Ball::destroy() {
    kill();
    if (Game::me->balls.empty()) Game::me->killPad();
}

void Ball::kill() {
    hx::remove(Game::me->balls, this);
    Element::kill();
}

// ------------------------------------------------------------------ Molecule

int Molecule::SUM = 0;
const Molecule::Info Molecule::INFOS[7] = {
    {15, {0, 1, 2}},  // SPEEDER
    {2, {0, 3, 3}},   // BUILDER
    {5, {4, 5, 4}},   // CLEANER
    {7, {5, 1, 5}},   // SPACER
    {5, {6, 2, 1}},   // ARMORER
    {40, {2, 3, 6}},  // FIXER
    {10, {2, 5, 2}},  // LIGHTER
};

Molecule::Molecule(MC) : Element(Game::me->dm->empty(Game::DP_MONSTER)) {
    // the original pushes into Game.molecules before calling super()
    Game::me->molecules.push_back(this);
    ray = 12;
    decal = hx::random(628);
    speed = 1;
    double a = hx::random() * 6.28;
    vx = std::cos(a) * speed;
    vy = std::sin(a) * speed;
    yLim = hx::Int(std::min((double)Game::me->level->ymax + 2, (double)Cs::YMAX - 1));
    setSpeed(1);
}

void Molecule::setType(int n) {
    type = n;
    if (type == CLEANER) setSpeed(1.5);
    mdm = std::make_unique<mt::DepthManager>(root);
    list.clear();
    for (int m : INFOS[type].mols) {
        Mol mol;
        mol.sp = new Sprite(mdm->attach("mcSpeederBall", 0));
        mol.sp->root->gotoAndStop(m + 1);
        list.push_back(mol);
        newTrg(list.back());
    }
}

void Molecule::update() {
    Element::update();
    if (type == SPACER) {
        for (auto* b : Game::me->balls) {
            double dist = getDist(b->x, b->y);
            double lim = 100;
            if (dist < lim) {
                double c = 1 - dist / lim;
                double a = b->getAng(x, y);
                double ba = std::atan2(b->vy, b->vx);
                double da = Num::hMod(ba - a, 3.14);
                b->setAngle(Num::hMod(ba + da * 0.1 * c, 3.14));
            }
        }
    }
    for (auto& m : list) {
        double dx = m.tx - m.sp->x, dy = m.ty - m.sp->y;
        if (std::fabs(dx) + std::fabs(dy) < 0.2 || hx::random(20) == 0) newTrg(m);
        m.sp->toward(m.tx, m.ty, 0.5, 1);
    }
    decal = std::fmod(decal + 35, 628);
    double c = 1 + std::cos(decal * 0.01) * 0.5;
    root->filters.clear();
    Filt::glow(root.get(), 2 + 5 * c, 1 + 2 * c, 0xFFFFFF);
}

void Molecule::newTrg(Mol& m) {
    double a = hx::random() * 6.28;
    double r = std::pow(hx::random(), 0.5) * 6;
    m.tx = std::cos(a) * r;
    m.ty = std::sin(a) * r;
    for (auto& o : list)
        if (o.sp != m.sp && m.sp->getDist(o.sp->x, o.sp->y) < 6) return;
    mdm->over(m.sp->root);
}

void Molecule::damage(Ball* ball) {
    switch (type) {
        case SPEEDER:
            if (ball) ball->setSpeed(ball->speed + 2);
            break;
        case FIXER:
            if (ball) ball->setType(Cs::BALL_STANDARD);
            break;
        case LIGHTER:
            if (Cs::pi->shopItems.get(ShopInfo::SUNGLASSES) != 1) Game::me->setFlash(1.4, -0.05);
            else Game::me->setFlash(0.7, -0.07);
            break;
    }
    if (ball && ball->fam >= 2) explode();
    for (auto& m : list) {
        double a = std::atan2(m.sp->y, m.sp->x);
        ray = 12;
        m.sp->x = std::cos(a) * ray;
        m.sp->y = std::sin(a) * ray;
        m.sp->updatePos();
    }
}

void Molecule::onEnterSquare(int sx, int sy) {
    if (flDeath) return;
    removeFromGrid(px - sx, py - sy);
    insertInGrid(px, py);
    if (py >= yLim) vy = -std::fabs(vy);
    if (type == BUILDER) {
        if (py + 1 < yLim) {
            int bx = px - sx, by = py - sy;
            if (!Game::me->gridAt(bx, by)) {
                Block* bl = new Block(bx, by, 0);
                bl->fxBlink();
            }
        }
    }
}

void Molecule::onBounce(int bpx, int bpy) {
    switch (type) {
        case CLEANER:
            for (int dx = 0; dx < 3; dx++)
                for (int dy = 0; dy < 3; dy++) {
                    Block* bl = Game::me->gridAt(bpx + dx - 1, bpy + dy - 1);
                    if (bl) bl->explode();
                }
            break;
        case ARMORER: {
            Block* bl = Game::me->gridAt(bpx, bpy);
            if (bl && bl->life < 5 && bl->type < 5) {
                bl->setLife(bl->life + 1);
                bl->fxBlink();
            }
            break;
        }
    }
    Element::onBounce(bpx, bpy);
}

void Molecule::setSpeed(double n) {
    speed = n;
    double a = std::atan2(vy, vx);
    vx = std::cos(a) * speed;
    vy = std::sin(a) * speed;
}

void Molecule::setAngle(double a) {
    vx = std::cos(a) * speed;
    vy = std::sin(a) * speed;
}

void Molecule::insertInGrid(int gx, int gy) {
    for (int dx = 0; dx < 3; dx++)
        for (int dy = 0; dy < 3; dy++)
            if (auto* m = Game::me->monsters(gx + dx - 1, gy + dy - 1)) m->push_back(this);
}

void Molecule::removeFromGrid(int gx, int gy) {
    for (int dx = 0; dx < 3; dx++)
        for (int dy = 0; dy < 3; dy++)
            if (auto* m = Game::me->monsters(gx + dx - 1, gy + dy - 1)) hx::remove(*m, this);
}

void Molecule::explode() {
    MC mc = Game::me->dm->attach("fxMonsterKill", Game::DP_MONSTER);
    flash::Point p = getPos();
    mc->x = p.x;
    mc->y = p.y;
    mc->xscale = mc->yscale = 70;
    mc->blendMode = flash::Blend::Add;
    kill();
}

void Molecule::kill() {
    flDeath = true;
    hx::remove(Game::me->molecules, this);
    removeFromGrid(px, py);
    Element::kill();
}

int Molecule::getRandomMolType(mt::OldRandom& seed) {
    if (SUM == 0)
        for (auto& o : INFOS) SUM += o.pb * 10;
    int rnd = seed.random(SUM);
    int sum = 0;
    for (int id = 0; id < 7; id++) {
        sum += hx::Int(INFOS[id].pb * 10);
        if (sum > rnd) return id;
    }
    return -1;
}

// ------------------------------------------------------------------ Shot

Shot::Shot(MC mc) : Element(std::move(mc)) { damage = 1; }

void Shot::onEnterSquare(int, int) {
    auto* ms = Game::me->monsters(px, py);
    Molecule* mon = (ms && !ms->empty()) ? (*ms)[0] : nullptr;
    if (mon) {
        flash::Point mp = mon->getPos(), bp = getPos();
        double dx = bp.x - mp.x, dy = bp.y - mp.y;
        if (std::sqrt(dx * dx + dy * dy) < mon->ray + 3) {
            mon->damage(nullptr);
            hit();
        }
    }
}

void Shot::onBounce(int bpx, int bpy) {
    Game::me->hit(bpx, bpy, this);
    hit();
    Element::onBounce(bpx, bpy);
}

void Shot::hit() { kill(); }

namespace shot {

Laser::Laser(MC mc) : Shot(std::move(mc)) {
    height = 44;
    if (auto s = root->smc()) s->yscale = 0;
    flHit = false;
}

void Laser::update() {
    Shot::update();
    double sens = flHit ? -1 : 1;
    if (auto s = root->smc()) {
        s->yscale = Num::mm(0, s->yscale + vit * mt::Timer::tmod * sens, height);
        if (flHit && s->yscale == 0) kill();
    }
    Game::me->plasmaDraw(root.get());
}

void Laser::setVit(double n) {
    vit = n;
    vy = -n;
}

void Laser::hit() {
    flHit = true;
    vy = 0;
}

Missile::Missile(MC mc) : Shot(std::move(mc)) {
    damage = 2;
    frict = 0.98;
}

void Missile::setType(int n) {
    root->gotoAndStop(n + 1);
    switch (n) {
        case 1: splash = 1; break;
        case 2:
            splash = 1;
            damage = 100;
            break;
        case 3:
            splash = 2;
            damage = 100;
            break;
    }
}

void Missile::update() {
    Shot::update();
    vy -= 1 * mt::Timer::tmod;
    auto* p = new Phys(Game::me->dm->attach("mcSmoke", Game::DP_PARTS2));
    p->x = x;
    p->y = y + 8;
    p->vy = -hx::random();
    p->frict = 0.92;
    p->timer = 14 + hx::random() * 4;
    p->root->rotation = hx::random() * 360;
    p->vr = (hx::random() * 2 - 1) * 5;
    p->setScale(100 + hx::random(40));
    Filt::blur(p->root.get(), 8, 8);
}

void Missile::onBounce(int bpx, int bpy) {
    if (splash >= 0) {
        int max = splash * 2 + 1;
        for (int dx = 0; dx < max; dx++)
            for (int dy = 0; dy < max; dy++) {
                Game::me->hit(bpx + dx - splash, bpy + dy - splash, this);
                Game::me->killZone(bpx + dx - splash, bpy + dy - splash);
            }
    } else {
        Game::me->hit(bpx, bpy, this);
    }
    hit();
    Shot::onBounce(bpx, bpy);  // super = Shot: hits the centre block a second time, like the original
}

}  // namespace shot
}  // namespace el
