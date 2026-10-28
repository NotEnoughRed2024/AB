// Block.hx
#include <cmath>

#include "Elements.h"

namespace Col = mt::Col;
namespace Filt = mt::Filt;
namespace Num = mt::Num;

bool Block::isSoft(const hx::Cell& c) {
    if (c.empty()) return true;  // undefined/null compare unequal to every constant
    int n = (int)c.v;
    return n != STEEL && n != SWAP && n != DOOR && !isSentinelle(n);
}

Block::Block(int px, int py, int t, MC mc, bool fe) : flEdit(fe), x(px), y(py) {
    Game::me->addBlock(this);
    register_();
    flIce = false;
    transform = 0;
    root = mc;
    if (!root) root = Game::me->bdm->attach("mcBlock", 0);
    root->x = Cs::getX(x);
    root->y = Cs::getY(y);
    if (t >= 0) setType(t);
}

void Block::setType(int t) {
    type = t;
    if (event) event->kill();
    event = nullptr;
    molType = -1;
    life = 0;
    if (type < BONUS) {
        if (type == 0) setStandard();
        life = std::min((double)type, 5.0);
        color = {Game::me->level->paint(x, y)};
        if (life >= 1) color = {0x885533};
    } else if (type <= BONUS + 3) {
        int id = type - 10;
        life = 0;
        // id 3 reads past the end of a 3-entry array in the original: [undefined], which tints black
        static const int cols[4] = {0xB3FD02, 0x0BCDFD, 0xFF5599, 0};
        color = {cols[id]};
    } else if (type >= CAGE && type < CAGE + MOLECULE_MAX) {
        molType = type - CAGE;
    } else if (type >= GENERATOR && type < GENERATOR + MOLECULE_MAX) {
        molType = type - GENERATOR;
        event = new ev::Generator(this);
        color = {0x663399, 0x950DD0, 0xFF55EE};
    }
    switch (type) {
        case SPACE: color = {0x7711FF}; break;
        case REDUC:
            color = {0x759BA4, 0x759BA4, 0x00F0FF};
            life = hx::NUL;
            onDamage = &Block::shootReduc;
            break;
        case STEEL:
            color = {0x759BA4};
            life = hx::NUL;
            break;
        case PUSHER:
            color = {0x78D816, 0xFF0000};
            life = 6;
            onDamage = &Block::shootGenerator;
            break;
        case JUMPER:
            color = {0xFFCC00, 0xFF0000};
            life = 3;
            onDamage = &Block::shootJumper;
            break;
        case STORM:
            color = {0x759BA4, 0x759BA4, 0xFFFF00};
            life = hx::NUL;
            onDamage = &Block::shootStorm;
            break;
        case ITEM:
            setStandard();
            color = {0x69B43D};
            break;
        case LURE: color = {0x759BA4}; break;
        case DRAGON_LEFT:
        case DRAGON_RIGHT: color = {0x376617, 0x5EAC28}; break;
        case INSECT: {
            color = {Game::me->level->paint(x, y)};
            dm = std::make_unique<mt::DepthManager>(root);
            double ma = 2;
            for (int i = 0; i < 8; i++) {
                MC mc = dm->attach("mcInsectBlock", 0);
                mc->x = ma + hx::random() * (Cs::BW - 2 * ma);
                mc->y = ma + hx::random() * (Cs::BH - 2 * ma);
                mc->stop();
                if (auto s = mc->smc()) s->stop();
            }
            break;
        }
        case SWAP:
            color = {0xCCFFCC, 0x0C7C2D};
            life = hx::NUL;
            onDamage = &Block::shootSwap;
            break;
        case MISSILE: color = {0x444466}; break;
        case DOOR: color = {0xF2C204}; break;
        case DEPLETED: color = {0x5D684C}; break;
        case NUT:
            color = {0xF1D89E, 0xFAE8C0};
            life = 1;
            break;
        case KILL:
            color = {0x759BA4, 0x759BA4, 0xFF0000};
            life = hx::NUL;
            onDamage = &Block::shootKill;
            break;
        case LIFE: color = {0xFFFFFF, 0xFF0000}; break;
        case DEATH: color = {0x423968, 0x141221, 0xFFFFFF}; break;
        case GLUE:
            color = {0x759BA4, 0x759BA4, 0xFF8800};
            life = hx::NUL;
            onDamage = &Block::shootGlue;
            break;
        case GUARDIAN: color = {0x996699, 0x6B476B, 0xFF9122}; break;
        case MINE: color = {Game::me->level->paint(x, y)}; break;
    }
    if (!hx::isNull(life)) life += 0.9;
    setSkin(root);
}

void Block::setStandard() {
    if (flStandard) return;
    Game::me->block++;
    flStandard = true;
}

void Block::setColor(const std::vector<int>& col) {
    color = col;
    if (auto s = root->smc())
        if (auto ss = s->smc())
            if (!color.empty()) Col::setColor(ss.get(), color[0]);
}

void Block::setLife(double n) {
    life = n;
    if (auto s = root->smc()) s->gotoAndStop(hx::Int(life) + 1);
    if (!color.empty()) setColor(color);
}

void Block::setSkin(const MC& mc) {
    if (type <= 5) mc->gotoAndStop(1);
    else if (type <= 12) mc->gotoAndStop(type - 10 + 2);
    else mc->gotoAndStop(type - 8);
    if (auto s = mc->smc()) {
        s->gotoAndStop(hx::Int(life) + 1);
        if (auto ss = s->smc())
            if (!color.empty()) Col::setColor(ss.get(), color[0]);
    }
    if (molType >= 0) {
        const auto& o = el::Molecule::INFOS[molType];
        for (int id = 0; id < 3; id++) {
            // the original looks these up on `root`, even when skinning another clip (Quasar)
            if (auto m = root->clip("_mc" + std::to_string(id))) m->gotoAndStop(o.mols[id] + 1);
        }
    }
}

void Block::incTransform(double n, int nt) {
    if (flDeath) return;
    transform = std::min(transform + n * mt::Timer::tmod, 1.0);
    if (transform == 1) {
        if (isBonus(type)) {
            explode();
        } else {
            if (dm) dm->clear(0);
            onDamage = nullptr;
            setType(nt);
        }
    }
}

void Block::damage(Hitter* ball) {
    if (flIce) {
        explode();
        return;
    }
    if (ball->hType() == Cs::BALL_ICE) {
        iceIt();
        return;
    }
    int ol = hx::Int(life);
    if (ball->hType() != Cs::BALL_VOLT && onDamage) (this->*onDamage)(ball);
    // WEAK && INDESTRUCTIBLE
    if ((ball->hFam() == 0 && life >= 1) || hx::isNull(life)) {
        if (ball->hDamage() == 100) explode();
        else fxBlink();
        return;
    }
    if (type == DEATH) ball->hDestroy();
    double n = ball->hDamage();
    if (life >= n) {
        setLife(life - n);
        if (ol != hx::Int(life)) fxBlink();
    } else {
        explode();
    }
}

void Block::iceIt() {
    type = 0;
    flIce = true;
    MC mc = mt::DepthManager(root).attach("mcIce", 0);
    mc->gotoAndStop(hx::random(mc->totalFrames()) + 1);
    int nx = hx::random(2), ny = hx::random(2);
    mc->xscale = (nx * 2 - 1) * 100;
    mc->yscale = (ny * 2 - 1) * 100;
    mc->x = (1 - nx) * Cs::BW;
    mc->y = (1 - ny) * Cs::BH;
}

void Block::explode() {
    Game* g = Game::me;
    if (type == SPACE) {
        unregister();
        setUpdate([this] { fadeOut(); });
        respawnTimer = 600;
        return;
    }
    if (type == DOOR) {
        unregister();
        setUpdate([this] { fadeOutVert(); });
        return;
    }
    int max = hx::Int(Num::mm(2, 18 - Sprite::spriteList.size() * 0.25, 16) * Cs::PREF_GFX);
    if (type >= BONUS && type <= BONUS + BONUS_MAX) fxTwinkle(max);
    else fxExplode(max);
    // OPTION
    if (flStandard && type != ITEM && !g->level->bonusTable.empty()) {
        int opt = g->level->bonusTable[x][y];
        if (opt >= 0 && (int)g->options.size() < Cs::MAX_OPTION) {
            g->level->bonusTable[x][y] = -1;
            g->newOption(opt, Cs::getX(x + 0.5), Cs::getY(y + 0.5));
        }
    }
    switch (type) {
        case BONUS: g->incMinerai(MIN_GREEN); break;
        case BONUS + 1: g->incMinerai(MIN_BLUE); break;
        case BONUS + 2: g->incMinerai(MIN_PINK); break;
        case DRAGON_LEFT: new ev::Dragon(this, -1); break;
        case DRAGON_RIGHT: new ev::Dragon(this, 1); break;
        case BALL: {
            el::Ball* b = g->newBall();
            b->moveTo(root->x + Cs::BW * 0.5, root->y + Cs::BH * 0.5);
            b->setAngle(hx::random() * 6.28);
            fxGlass(max);
            break;
        }
        case BOOM:
            explodeTimer = 4;
            setUpdate([this] { updateExplode(); });
            fxBoom();
            break;
        case MINE: {
            fxBoom();
            int ray = 1;
            for (int dx = 0; dx < ray * 2 + 1; dx++)
                for (int dy = 0; dy < ray * 2 + 1; dy++) {
                    Block* bl = g->gridAt(x + dx - ray, y + dy - ray);
                    if (bl && bl != this && !bl->flDeath) {
                        bl->flDeath = true;
                        bl->explode();
                    }
                }
            break;
        }
        case INSECT:
            for (int i = 0; i < 10; i++) {
                double a = hx::random() * 6.28;
                double speed = 0.5 + hx::random() * 6;
                auto* sp = new fx::Fly();
                sp->x = Cs::getX(x + hx::random());
                sp->y = Cs::getY(y + hx::random());
                sp->vx = std::cos(a) * speed;
                sp->vy = std::sin(a) * speed;
                if (i > 1) sp->timer = 10 + hx::random() * 20;
            }
            break;
        case MISSILE:
            if (Cs::pi->missileMax > 0) g->newOption(26, Cs::getX(x + 0.5), Cs::getY(y + 0.5));
            break;
        case NUT: {
            auto* shot = new BadShot(g->dm->attach("mcBadShot", Game::DP_PARTS));
            shot->vr = 37;
            shot->setType(1);
            shot->frontShot(this, -5);
            shot->weight = 1;
            shot->vx = (hx::random() * 2 - 1) * 0.8;
            Filt::glow(shot->root.get(), 2, 4, 0xFFFFFF);
            Filt::glow(shot->root.get(), 10, 1, 0xFFFFFFF & 0xFFFFFF);
            break;
        }
        case LIFE:
            g->newLife(g->life, true);
            g->life += 1;
            fxSparks(24, 3, 8, 100);
            break;
        case GUARDIAN: {
            auto* shot = new BadShot(g->dm->attach("mcBadShot", Game::DP_PARTS));
            shot->directShot(this, 36);
            shot->height = std::fabs(shot->vy);
            shot->setType(4);
            break;
        }
    }
    if (molType >= 0) {
        int m = 0;
        if (type < GENERATOR) {
            m = 1;
            fxGlass(m);
        }
        for (int i = 0; i < m; i++) genMolecule();
    }
    if (flIce) {
        for (int n = 0; n < hx::Int(max * 0.5); n++) {
            auto* p = new Phys(g->dm->attach("partIceShard", Game::DP_PARTS));
            initExplode(p);
            p->weight *= 0.5;
            p->root->rotation = std::atan2(p->vy, p->vx) / 0.0174;
            p->vr = (hx::random() * 2 - 1) * 6;
        }
    }
    if (type == ITEM) {
        auto* opt = new Option(g->dm->attach("optItem", Game::DP_OPTION));
        opt->x = Cs::getX(x + 0.5);
        opt->y = Cs::getY(y + 0.5);
        opt->setItem(g->level->itemId);
    }
    kill();
}

// SPECIALS
void Block::shootReduc(Hitter*) {
    Game* g = Game::me;
    if (!g->pad) return;
    if (g->pad->ray == Pad::SIDE + 1) return;
    MC mc = g->dm->attach("fxReduc", Game::DP_PARTS);
    mc->x = Cs::getX(x + 0.5);
    mc->y = Cs::getY(y + 0.5);
    auto* shot = new BadShot(g->dm->attach("mcBadShot", Game::DP_PARTS));
    shot->directShot(this, 12);
    shot->setType(0);
    shot->height = std::fabs(shot->vy * 10);
}

void Block::shootGenerator(Hitter* ball) {
    if (!ball) return;
    Game* g = Game::me;
    int dx = (x - ball->hPx());
    int dy = (y - ball->hPy());
    int n = 0;
    std::vector<Block*> list;
    bool flAbort = false;
    while (true) {
        n++;
        int nx = x + dx * n, ny = y + dy * n;
        if (nx < 0 || nx >= Cs::XMAX || ny < 0 || ny >= Cs::YMAX) {
            flAbort = true;
            break;
        }
        Block* bl = g->gridAt(nx, ny);
        if (!bl) break;
        list.insert(list.begin(), bl);
        if (n > 64) break;  // diagonal hits (dx and dy both 0) would loop forever; Flash would hang too
    }
    if (!flAbort) {
        for (Block* bl : list) {
            bl->unregister();
            bl->setPos(bl->x + dx, bl->y + dy);
            bl->register_();
        }
        new Block(x + dx, y + dy, 0);
    }
}

void Block::shootJumper(Hitter* ball) {
    if (life - ball->hDamage() < 0) return;
    unregister();
    root->visible = false;
    auto* sp = new fx::Jumper(Game::me->dm->attach("mcJumper", Game::DP_PARTS));
    sp->x = Cs::getX(x + 0.5);
    sp->y = Cs::getY(y + 0.5);
    sp->bl = this;
    sp->root->gotoAndStop((int)life);
    sp->seekTrg();
}

void Block::shootStorm(Hitter*) {
    if (!Game::me->pad || ev::Storm::TOTAL >= 3) return;
    if (!hx::isNull(lastShot) && hx::getTimer() - lastShot < 1000) return;
    auto* e = new ev::Storm();
    e->bl = this;
    lastShot = hx::getTimer();
}

void Block::shootSwap(Hitter*) { Game::me->swapScreen(); }

void Block::shootKill(Hitter*) {
    Game* g = Game::me;
    if (!g->pad) return;
    if (!hx::isNull(lastShot) && hx::getTimer() - lastShot < 1000) return;
    MC mc = g->dm->attach("fxKill", Game::DP_PARTS);
    mc->x = Cs::getX(x + 0.5);
    mc->y = Cs::getY(y + 0.5);
    for (int i = 0; i < 8; i++) {
        auto* shot = new BadShot(g->dm->attach("mcBadShot", Game::DP_PARTS));
        shot->bl = this;
        shot->sleep = i * 4;
        shot->setType(2);
    }
    lastShot = hx::getTimer();
}

void Block::shootGlue(Hitter*) {
    Game* g = Game::me;
    if (!g->pad) return;
    auto* shot = new BadShot(g->dm->attach("mcBadShot", Game::DP_PARTS));
    shot->directShot(this, 8 + hx::random() * 4, 0.3);
    shot->setType(3);
    if (auto s = shot->root->smc()) s->gotoAndPlay(hx::random(10));
}

void Block::updateExplode() {
    explodeTimer--;
    int ray = hx::Int(2 - explodeTimer * 0.5);
    for (int dx = 0; dx < ray * 2 + 1; dx++)
        for (int dy = 0; dy < ray * 2 + 1; dy++) {
            Block* bl = Game::me->gridAt(x + dx - ray, y + dy - ray);
            if (bl) bl->explode();
        }
    if (explodeTimer == 0) removeUpdate();
}

void Block::genMolecule() {
    auto* mon = new el::Molecule(nullptr);
    mon->setType(molType);
    mon->moveTo(root->x + Cs::BW * 0.5, root->y + Cs::BH * 0.5);
}

// CRAWLERS
void Block::setUpdate(std::function<void()> f) {
    if (update) hx::remove(Game::me->crawlers, this);
    update = std::move(f);
    Game::me->crawlers.push_back(this);
}

void Block::removeUpdate() {
    update = nullptr;
    hx::remove(Game::me->crawlers, this);
}

void Block::fadeOut() {
    if (root->alpha > 0) root->alpha -= 10;
    respawnTimer -= mt::Timer::tmod;
    if (respawnTimer <= 0) {
        if (!Game::me->gridAt(x, y)) {
            register_();
            setUpdate([this] { fadeIn(); });
        }
    }
}

void Block::fadeIn() {
    if (root->alpha < 100) root->alpha += 10;
    else removeUpdate();
}

void Block::fadeOutVert() {
    if (root->yscale > 0) root->yscale *= 0.75;
    bool flOk = false;
    for (auto* b : Game::me->balls)
        if (b->py < y) {
            flOk = true;
            break;
        }
    if (!flOk) return;
    if (!Game::me->gridAt(x, y)) {
        register_();
        setUpdate([this] { fadeInVert(); });
    }
}

void Block::fadeInVert() {
    if (root->yscale < 100) root->yscale = std::min(root->yscale + 10, 100.0);
    else removeUpdate();
}

void Block::register_() {
    flDeath = false;
    Game::me->blocks.push_back(this);
    Game::me->gridSet(x, y, this);
}

void Block::unregister() {
    flDeath = true;
    hx::remove(Game::me->blocks, this);
    Game::me->gridSet(x, y, nullptr);
}

void Block::setPos(int px, int py) {
    x = px;
    y = py;
    root->x = Cs::getX(x);
    root->y = Cs::getY(y);
}

// FX
void Block::fxBlink() {
    MC mc = Game::me->dm->attach("mcBlink", Game::DP_BLOCK);
    mc->x = root->x;
    mc->y = root->y;
    blink = mc;
    mc->blendMode = flash::Blend::Add;
    Filt::glow(mc.get(), 10, 1, 0xFFFFFF);
}

void Block::fxGlass(int max) {
    for (int n = 0; n < max; n++) {
        auto* p = new fx::Part(Game::me->dm->attach("partGlass", Game::DP_PARTS));
        initExplode(p);
        p->bouncer->setPos(p->x, p->y);
        p->root->rotation = hx::random() * 2 - 1;
        p->vr = (hx::random() * 2 - 1) * 12;
        p->setScale(p->scale * (1 + hx::random() * 0.6));
        p->root->gotoAndStop(hx::random(p->root->totalFrames()) + 1);
        p->updatePos();
    }
}

void Block::fxSparks(int max, double cr, double spMax, double randSize) {
    if (hx::isNull(cr)) cr = 3;
    if (hx::isNull(spMax)) cr = spMax;  // sic: the original assigns cr here, leaving spMax null
    if (hx::isNull(randSize)) randSize = 0;
    for (int i = 0; i < max; i++) {
        double a = ((i + hx::random()) / max) * 6.28;
        double ca = std::cos(a), sa = std::sin(a);
        double sp = hx::random() * spMax;  // NaN when spMax was null -> particles get NaN positions (invisible)
        auto* p = new Phys(Game::me->dm->attach("partSpark", Game::DP_PARTS));
        p->x = root->x + Cs::BW * 0.5 + ca * sp * cr * 1.5;
        p->y = root->y + Cs::BH * 0.5 + sa * sp * cr;
        p->vx = ca * sp * 1.5;
        p->vy = sa * sp;
        p->timer = 10 + hx::random() * 10;
        p->fadeType = 0;
        p->updatePos();
        p->root->gotoAndStop(3);
        if (auto s = p->root->smc()) s->gotoAndPlay(hx::random(2) + 1);
        p->frict = 0.9;
        p->setScale(100 + hx::random() * randSize);
    }
}

void Block::fxBolt() {
    MC mc = Game::me->dm->attach("mcBolt", Game::DP_PARTS);
    mc->xscale = mc->yscale = 50 + hx::random() * 100;
    Filt::glow(mc.get(), 10, 1, 0xFFFF00);
    mc->blendMode = flash::Blend::Add;
    mc->x = Cs::getX(x + hx::random());
    mc->y = Cs::getY(y + hx::random());
    mc->rotation = hx::random() * 360;
}

void Block::fxTwinkle(int max) {
    for (int i = 0; i < max; i++) {
        auto* p = new Phys(Game::me->dm->attach("partTwinkle", Game::DP_PARTS));
        double a = (double)i / max * 6.28;
        double ray = 5 + hx::random() * 20;
        p->x = Cs::getX(x + 0.5) + std::cos(a) * ray;
        p->y = Cs::getY(y + 0.5) + std::sin(a) * ray;
        p->timer = 10 + hx::random() * 10;
        p->fadeType = 0;
        p->setScale(50 + hx::random() * 100);
        p->sleep = hx::random() * (ray - 5);
        p->vy -= hx::random();
        p->root->blendMode = flash::Blend::Add;
        p->root->gotoAndPlay(hx::random(2) + 1);
        p->updatePos();
    }
}

void Block::fxExplode(int max) {
    Game* g = Game::me;
    MC mc = g->dm->attach("partExplode", Game::DP_PARTS);
    mc->x = root->x;
    mc->y = root->y;
    mc->xscale = Cs::BW / 30.0 * 100;
    mc->yscale = Cs::BH / 10.0 * 100;
    if (!color.empty()) Col::setColor(mc.get(), color[0]);
    for (int n = 0; n < max; n++) {
        auto* p = new fx::Part(g->dm->attach("mcPart", Game::DP_PARTS));
        initExplode(p);
        p->bouncer->setPos(p->x, p->y);
        p->updatePos();
        if (!color.empty()) Col::setColor(p->root.get(), color[hx::random((int)color.size())]);
    }
}

void Block::fxFrout(int max) {
    for (int i = 0; i < max; i++) {
        auto* p = new Phys(Game::me->dm->attach("mcPart", Game::DP_PARTS));
        p->x = Cs::getX(x + hx::random());
        p->y = Cs::getY(y + hx::random());
        p->weight = 0.07 + hx::random() * 0.1;
        p->fadeType = 0;
        p->frict = 0.98;
        p->setScale(p->weight * 700);
        p->timer = 10 + hx::random() * 10;
        if (!color.empty()) Col::setColor(p->root.get(), color[hx::random((int)color.size())]);
    }
}

void Block::fxBoom() {
    auto* sp = new fx::Tracer(Game::me->dm->attach("partBoom", Game::DP_PARTS));
    sp->x = Cs::getX(x + 0.5);
    sp->y = Cs::getY(y + 0.5);
    sp->root->blendMode = flash::Blend::Add;
    sp->timer = 5;
    sp->fadeType = -1;
    sp->updatePos();
}

void Block::initExplode(Phys* p) {
    double cx = root->x + Cs::BW * 0.5;
    double cy = root->y + Cs::BH * 0.5;
    p->x = Cs::getX(x + hx::random());
    p->y = Cs::getY(y + hx::random());
    double dx = p->x - cx, dy = p->y - cy;
    double a = std::atan2(dy, dx);
    double sp = std::sqrt(dx * dx + dy * dy) * 0.2;
    p->vx = std::cos(a) * sp;
    p->vy = std::sin(a) * sp;
    p->timer = 10 + hx::random() * 20;
    p->weight = 0.07 + hx::random() * 0.1;
    p->fadeType = 0;
    p->frict = 0.98;
    p->setScale(p->weight * 700);
}

// KILL
void Block::kill() {
    if (event) event->kill();
    unregister();
    if (flStandard) Game::me->removeBlock();
    if (blink && blink->visible && !blink->removed) blink->removeMovieClip();
    root->removeMovieClip();
}
