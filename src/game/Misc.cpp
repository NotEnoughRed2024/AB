// Option.hx, BadShot.hx, ev/*.hx, fx/*.hx
#include <cmath>
#include <map>

#include "Elements.h"

namespace Col = mt::Col;
namespace Filt = mt::Filt;
namespace Num = mt::Num;

// ------------------------------------------------------------------ Option

int Option::SUM = 0;
const double Option::PROB[26] = {5, 1, 12, 10, 30, 10, 7, 5, 5, 10, 0.5, 5, 36,
                                 15, 3, 0, 2, 2, 3, 10, 0, 10, 2, 5, 1, 5};
std::vector<double> Option::prob;

Option::Option(MC mc) : Phys(std::move(mc)) {
    Game::me->options.push_back(this);
    vy = FALL_SPEED;
}

int Option::getRandomId(mt::OldRandom& seed) {
    int rnd = seed.random(SUM);
    int sum = 0;
    for (int i = 0; i < (int)prob.size(); i++) {
        sum += hx::Int(prob[i] * 10);
        if (sum > rnd) return i;
    }
    return -1;
}

void Option::genProb(const Level* level) {
    prob.assign(PROB, PROB + 26);
    prob[PROVISION] = Num::mm(10, 10 - level->dst * 0.5, 0.3);
    if (level->dst < 3) {
        prob[DIMINUTION] = 0;
        prob[WHISKY] = 0;
        prob[BLINDAGE] = 0;
        prob[ZELE] = 0;
        prob[YOYO] = 0;
        prob[INDIGESTION] = 0;
        prob[HALO] = 0;
    }
    if (level->dst < 4) prob[FLAMME] = 0;
    if (level->dst < 5) prob[PROVISION] = 0;
    if (level->dst < 6) prob[VOLT] = 0;
    if (level->dst < 8) prob[TRANSFORMEUR] = 0;
    if (level->dst < 10) prob[GLACE] = 0;
    SUM = 0;
    for (double n : prob) SUM += hx::Int(n * 10);
}

void Option::setType(int n) {
    id = n;
    int col = getCol(id);
    MC scroll = root->clip("scroll");
    if (auto s = root->smc()) Col::setColor(s.get(), col);
    if (scroll)
        if (auto s = scroll->smc()) Col::setColor(s.get(), col);
    Col::RGB o = Col::colToObj(col);
    int inc = -200;
    o.r = std::max(o.r + inc, 0);
    o.g = std::max(o.g + inc, 0);
    o.b = std::max(o.b + inc, 0);
    TF field = scroll ? scroll->field("field") : nullptr;
    if (field) Filt::glow(field.get(), 2, 20, 0xFFFFFF);
    color = col;
    if (n < 26) {
        if (scroll) scroll->stop();
        if (field) {
            field->setText(std::string(1, (char)(65 + n)));
            field->textColor = Col::objToCol(o);
        }
    } else {
        if (scroll) {
            scroll->gotoAndStop(2 + (n - 26));
            if (n == 26)
                if (auto icon = scroll->clip("icon")) icon->gotoAndStop(Game::me->missileType + 1);
        }
        if (auto s = root->smc()) Filt::glow(s.get(), 2, 4, 0xFFFFFF);
    }
}

void Option::setItem(int n) {
    Game::me->flItemFall = true;
    id = n;
    flItem = true;
    MC scroll = root->clip("scroll");
    if (scroll) scroll->gotoAndStop(id + 1);
    MC s = root->smc();
    if (s) {
        sdm = std::make_unique<mt::DepthManager>(s);
        s->blendMode = flash::Blend::Add;
    }
    for (int i = 0; i < 16; i++) newRay();
    Game::me->dm->over(root);
    if (s) Filt::glow(s.get(), 10, 2, 0xFFFFFF);
    if (scroll) {
        Filt::glow(scroll.get(), 5, 2, 0xFFFFFF, true);
        Filt::glow(scroll.get(), 10, 2, 0xFFFFFF);
    }
}

void Option::update() {
    Phys::update();
    if (flItem) {
        newRay();
    } else if (MC scroll = root->clip("scroll")) {
        scroll->y += mt::Timer::tmod;
        if (scroll->y > -5) scroll->y -= 26;
    }
    Pad* pad = Game::me->pad;
    if (pad && std::fabs(y - (pad->y + Cs::BH * 0.5)) < Cs::BH && std::fabs(x - pad->x) < pad->ray + Cs::BW * 0.5) {
        apply();
    }
    if (y > Cs::mch + 30) kill();
}

void Option::apply() {
    int max = hx::Int(24 * Cs::PREF_GFX);
    for (int i = 0; i < max; i++) {
        auto* p = new fx::LineUp(Game::me->dm->attach("partLineUp", Game::DP_PARTS));
        p->y = y + (hx::random() * 2 - 1) * Cs::BH * 0.5;
        p->x = x + (hx::random() * 2 - 1) * Cs::BW * 0.5;
        p->vx = (hx::random() * 2 - 1) * 5;
        p->factor = 8;
        p->timer = 10 + hx::random() * 10;
        p->frict = 0.9;
    }
    MC mc = Game::me->dm->attach("mcOnde", Game::DP_UNDERPARTS);
    mc->x = x;
    mc->y = y;
    if (flItem) {
        Game::me->flItemCollected = true;
        std::string name = Text::at("ITEM_NAMES", id);
        for (auto& ch : name) ch = (char)std::toupper((unsigned char)ch);
        Game::me->newTitle(name, 0x000044, false, 60);
    } else {
        Game::me->getOption(id);
    }
    kill();
}

void Option::newRay() {
    if (!sdm) return;
    auto* sp = new Phys(sdm->attach("mcRayWaveGrow", 0));
    sp->vr = (hx::random() * 2 - 1) * 1.5;
    sp->root->rotation = hx::random() * 360;
    sp->root->xscale = 30 + hx::random() * 40;
    sp->root->yscale = sp->scale = 40;
    sp->root->alpha = 50;
    sp->timer = 10 + hx::random() * 20;
    sp->fadeType = 3;
    sp->updatePos();
}

void Option::kill() {
    if (dead) return;
    if (flItem) Game::me->flItemFall = false;
    hx::remove(Game::me->options, this);
    Phys::kill();
}

int Option::getCol(int id) {
    if (id >= 26) return 0x444466;
    Col::RGB co = Col::getRainbow(id / 26.0);
    co.r = hx::Int(std::max(co.r - (id % 3) * 50.0, 0.0));
    co.g = hx::Int(std::max(co.g - (id % 3) * 50.0, 0.0));
    co.b = hx::Int(std::max(co.b - (id % 3) * 50.0, 0.0));
    return Col::objToCol(co);
}

bool Option::isBad(int id) {
    return id == 1 || id == 3 || id == 6 || id == 8 || id == 13 || id == 22 || id == 23 || id == 25;
}

// ------------------------------------------------------------------ BadShot

BadShot::BadShot(MC mc) : Phys(std::move(mc)) {
    flUp = true;
    ray = 2;
    yLim = Cs::mch + 10;
    height = 0;
}

void BadShot::update() {
    double ox = x, oy = y;
    Phys::update();
    Pad* pad = Game::me->pad;
    if (pad) {
        if (flUp && y + ray > pad->y) {
            impact = x;
            flUp = false;
        }
        if (!hx::isNull(impact)) {
            if (std::fabs(pad->x - impact) < pad->ray + ray) {
                hit();
                return;
            }
            if (y - pad->y > ray + height) impact = hx::NUL;
        }
    }
    if (!queue_.empty()) {
        MC mc = Game::me->dm->attach(queue_, Game::DP_PAD);
        mc->x = ox;
        mc->y = oy;
        double dx = x - ox, dy = y - oy;
        mc->rotation = std::atan2(dy, dx) / 0.0174;
        mc->xscale = std::sqrt(dx * dx + dy * dy);
    }
    if (type == 2 && hx::isNull(sleep) && vy == 0) {
        root->stop();
        if (bl) directShot(bl, 8);
    }
    if (y > yLim + height) kill();
}

void BadShot::setType(int n) {
    type = n;
    root->gotoAndStop(n + 1);
    switch (type) {
        case 0: queue_ = "mcQueueReduc"; break;
        case 1: queue_ = "mcQueueNut"; break;
        case 4: queue_ = "mcQueueDrone"; break;
    }
}

void BadShot::directShot(Block* b, double speed, double prec) {
    x = Cs::getX(b->x + 0.5);
    y = Cs::getY(b->y + 0.5);
    Pad* pad = Game::me->pad;
    if (!pad) return;
    double dx = pad->x - x, dy = pad->y - y;
    double a = std::atan2(dy, dx) + (hx::random() * 2 - 1) * prec;
    vx = std::cos(a) * speed;
    vy = std::sin(a) * speed;
}

void BadShot::frontShot(Block* b, double speed) {
    x = Cs::getX(b->x + 0.5);
    y = Cs::getY(b->y + 0.5);
    vy = speed;
}

void BadShot::hit() {
    Game* g = Game::me;
    switch (type) {
        case 0:
            g->pad->setRay(std::max(g->pad->ray - 8, (double)Pad::SIDE + 1));
            g->pad->setFlash(10);
            break;
        case 1:
        case 2:
        case 4:
            g->killPad();
            break;
        case 3: {
            g->pad->glueCount++;
            g->pad->moveFactor *= 0.5;
            int max = 16;
            double cr = 4;
            for (int n = 0; n < max; n++) {
                auto* p = new Phys(g->dm->attach("mcGlue", Game::DP_PARTS));
                double a = hx::random() * 6.28;
                double ca = std::cos(a), sa = std::sin(a);
                double sp = 0.1 + hx::random() * 5;
                p->x = x + ca * cr * sp;
                p->y = y + sa * cr * sp;
                p->vx = ca * sp;
                p->vy = -std::fabs(sa) * sp * 0.5;
                p->fadeType = 0;
                p->weight = 0.05 + hx::random() * 0.15;
                p->timer = 10 + hx::random() * 10;
                p->setScale(10 + hx::random() * 20);
            }
            Filt::glow(g->pad->root.get(), 6, 2, 0xFFFF00);
            break;
        }
    }
    kill();
}

// ------------------------------------------------------------------ events

namespace ev {

Generator::Generator(Block* b) : bl(b) { timer = 140 * 0.5; }

void Generator::update() {
    static const int MOLECULES_MAX = 6;
    static const double CYCLE = 140;
    Game* g = Game::me;
    MC s = bl->root->smc();
    if ((int)g->molecules.size() < MOLECULES_MAX && !bl->flIce) {
        timer -= mt::Timer::tmod;
        double lim = 30;
        if (timer <= lim) {
            if (timer > 3) {
                for (int i = 0; i < 3; i++) {
                    if (hx::random() * timer < 5) {
                        auto* p = new Phys(g->dm->attach("mcGenLine", Game::DP_PARTS));
                        p->x = bl->root->x + Cs::BW * 0.5;
                        p->y = bl->root->y + Cs::BH * 0.5;
                        p->root->rotation = hx::random() * 360;
                        p->timer = 5 + hx::random(8);
                        p->fadeLimit = p->timer;
                        p->fadeType = 0;
                        p->updatePos();
                        Filt::glow(p->root.get(), 10, 2, 0xCC88FF);
                        p->root->blendMode = flash::Blend::Add;
                    }
                }
            }
            if (s) s->xscale = s->yscale = 100 * (1 - timer / lim);
        }
        if (timer <= 0) {
            timer = CYCLE;
            if (s) s->xscale = s->yscale = 0;
            bl->genMolecule();
            bl->fxSparks(14);
        }
    } else {
        if (s) s->xscale = s->yscale = 0;
    }
}

Indigestion::Indigestion() {
    Game* g = Game::me;
    std::vector<std::vector<bool>> grid(Cs::XMAX + 2, std::vector<bool>(Cs::YMAX + 2, false));
    for (int x = 0; x < Cs::XMAX; x++)
        for (int y = 0; y < Cs::YMAX; y++)
            if (g->gridAt(x, y))
                for (auto& d : Cs::DIR) {
                    int nx = x + d[0], ny = y + d[1];
                    // grid[-1] is undefined in the original, so writes there are lost
                    if (nx >= 0 && ny >= 0 && !g->gridAt(nx, ny)) grid[nx][ny] = true;
                }
    for (int x = 0; x < Cs::XMAX; x++)
        for (int y = 0; y < Cs::YMAX; y++)
            if (grid[x][y]) new Block(x, y, 0);
    kill();
}

Javelot::Javelot() {
    Game* g = Game::me;
    x = Cs::getPX(g->pad->x);
    y = Cs::YMAX + 4;
    mcJavelot = g->dm->attach("mcJavelot", Game::DP_PAD);
    mcJavelot->x = Cs::getX(x + 0.5);
    mcJavelot->y = Cs::getY(y);
    mcJavelot->blendMode = flash::Blend::Add;
    g->dm->under(mcJavelot);
}

void Javelot::update() {
    static const int SPEED = 5;
    Game* g = Game::me;
    for (int i = 0; i < SPEED; i++) {
        y--;
        if (Block* bl = g->gridAt(x, y)) bl->explode();
        g->killZone(x, y);
    }
    mcJavelot->y = Cs::getY(y);
    int max = hx::Int(3 + Cs::getPerfCoef() * 8);
    double hh = Cs::BH * SPEED;
    for (int i = 0; i < max; i++) {
        auto* p = new Phys(g->dm->attach("partLight", Game::DP_PARTS));
        p->x = mcJavelot->x + (hx::random() * 2 - 1) * 10;
        p->y = mcJavelot->y + hx::random() * hh;
        p->vy = -hx::random() * hh * 0.75;
        p->timer = 10 + hx::random() * 20;
    }
    if (y < -14) kill();
}

void Javelot::kill() {
    mcJavelot->removeMovieClip();
    Event::kill();
}

Ouverture::Ouverture() {
    range = 3;
    crush();
}

void Ouverture::update() {
    timer += mt::Timer::tmod;
    if (timer > 10) crush();
    if (range == 0) kill();
}

void Ouverture::crush() {
    Game* g = Game::me;
    auto list = g->blocks;
    for (Block* bl : list)
        if (bl->update) {
            bl->removeUpdate();
            bl->kill();
        }
    range--;
    for (int n = 0; n < 2; n++) {
        int sens = (n * 2 - 1);
        int max = hx::Int(Cs::XMAX * 0.5) + 1;
        for (int i = 0; i < max; i++) {
            int sx = Cs::XMAX * n - 1;
            int x = sx - i * sens;
            if (x < 0 || x >= (int)g->grid.size()) continue;
            auto column = g->grid[x];  // "for (bl in a)" iterates a snapshot of the slots
            for (size_t yy = 0; yy < column.size(); yy++) {
                Block* bl = g->grid[x].size() > yy ? g->grid[x][yy] : nullptr;
                if (!bl) continue;
                int nx = x + sens;
                if (nx >= 0 && nx < Cs::XMAX) {
                    bl->unregister();
                    bl->setPos(nx, bl->y);
                    bl->register_();
                    if (hx::random(3) == 0) bl->fxFrout(1);
                } else {
                    bl->kill();
                }
            }
        }
    }
    timer = 0;
    g->shake = 10;
}

Quasar::Quasar() {
    Game* g = Game::me;
    mcCenter = g->dm->attach("mcQuasar", Game::DP_UNDERPARTS);
    mcCenter->x = Cs::mcw * 0.5;
    mcCenter->y = 100;
    mcCenter->xscale = mcCenter->yscale = 0;
    Filt::blur(mcCenter.get(), 10, 10);
    timer = 60;
    for (int x = 0; x < Cs::XMAX; x++)
        for (int y = 0; y < Cs::YMAX; y++) {
            g->killZone(x, y);
            Block* bl = g->gridAt(x, y);
            if (bl) {
                double blx = Cs::getX(x + 0.5), bly = Cs::getY(y + 0.5);
                double dx = blx - mcCenter->x, dy = bly - mcCenter->y;
                double dist = std::sqrt(dx * dx + dy * dy);
                if (dist < RAY) {
                    MC mc = g->dm->empty(Game::DP_UNDERPARTS);
                    MC mc2 = mt::DepthManager(mc).attach("mcBlock", 0);
                    mc2->x = -Cs::BW * 0.5;
                    mc2->y = -Cs::BH * 0.5;
                    PartBlock& p = newPart(mc, blx, bly);
                    p.flBlock = true;
                    bl->setSkin(mc2);
                    bl->kill();
                }
            }
        }
}

void Quasar::update() {
    Game* g = Game::me;
    mcCenter->rotation += 1;
    timer -= mt::Timer::tmod;
    double c = timer / 10;
    auto lst = list;
    std::vector<Phys*> removed;
    for (auto& p : lst) {
        double dx = mcCenter->x - p.p->x, dy = mcCenter->y - p.p->y;
        double dist = std::sqrt(dx * dx + dy * dy);
        double ta = std::atan2(dy, dx);
        // work on the real entry
        PartBlock* rp = nullptr;
        for (auto& q : list)
            if (q.p == p.p) rp = &q;
        if (!rp) continue;
        rp->c = std::min(rp->c + rp->cs * mt::Timer::tmod, 1.0);
        rp->speed = std::min(rp->speed + rp->acc * mt::Timer::tmod, 5.0);
        double da = Num::hMod(ta - rp->a, 3.14);
        rp->a += da * rp->c * mt::Timer::tmod;
        rp->p->vx = std::cos(rp->a) * rp->speed;
        rp->p->vy = std::sin(rp->a) * rp->speed;
        double ds = std::min(dist, rp->p->scale) - rp->p->scale;
        rp->p->setScale(rp->p->scale + ds * 0.1 * mt::Timer::tmod);
        if (c < 1 && rp->flBlock) Col::setPercentColor(rp->p->root.get(), (1 - c) * 100, 0);
        if ((!rp->flBlock || c < 0.5) && dist < 30 * (1 - c)) {
            rp->p->kill();
            Phys* dead = rp->p;
            list.erase(std::remove_if(list.begin(), list.end(), [dead](const PartBlock& q) { return q.p == dead; }),
                       list.end());
        }
    }
    for (int i = 0; i < hx::Int(timer / 20); i++) {
        double a = hx::random() * 6.28;
        double r = hx::random() * 150;
        double x = mcCenter->x + std::cos(a) * r;
        double y = mcCenter->y + std::sin(a) * r;
        PartBlock& p = newPart(g->dm->attach("partLight", Game::DP_PARTS), x, y);
        p.acc = 0.4;
        p.cs = 0.001;
    }
    if (timer > 50) c = 1 - (timer - 50) / 10;
    if (c < 1) mcCenter->xscale = mcCenter->yscale = c * 100;
    if (timer < 0) kill();
}

Quasar::PartBlock& Quasar::newPart(MC mc, double x, double y) {
    double dx = x - mcCenter->x, dy = y - mcCenter->y;
    double a = std::atan2(dy, dx);
    PartBlock p;
    p.p = new Phys(mc);
    p.p->x = x;
    p.p->y = y;
    p.p->fadeType = 0;
    p.a = a + 1.57;
    p.c = 0.05;
    p.cs = 0.003;
    p.speed = 0;
    p.acc = 0.2;
    p.flBlock = false;
    p.p->updatePos();
    list.push_back(p);
    return list.back();
}

void Quasar::kill() {
    while (!list.empty()) {
        PartBlock p = list.back();
        list.pop_back();
        if (p.flBlock) p.p->kill();
        else p.p->timer = 10 + hx::random() * 10;
    }
    mcCenter->removeMovieClip();
    Event::kill();
}

Dragon::Dragon(Block* bl, int s) : sens(s) {
    Game* g = Game::me;
    x = bl->x;
    y = bl->y;
    mcDragon = g->dm->attach("mcDragon", Game::DP_FRONT_PARTS);
    mcDragon->x = Cs::getX(x + 0.5);
    mcDragon->y = Cs::getY(y + 0.5);
    mcDragon->xscale = 100 * sens;
    Filt::glow(mcDragon.get(), 10, 1, 0xFFFFFF);
    mcDragon->blendMode = flash::Blend::Add;
}

void Dragon::update() {
    static const int SPEED = 1;
    Game* g = Game::me;
    for (int i = 0; i < SPEED; i++) {
        x += sens;
        if (Block* bl = g->gridAt(x, y)) bl->explode();
        g->killZone(x, y);
    }
    mcDragon->x = Cs::getX(x + 0.5);
    int max = hx::Int(3 + Cs::getPerfCoef() * 8);
    double ww = Cs::BW * SPEED;
    for (int i = 0; i < max; i++) {
        auto* p = new Phys(g->dm->attach("partLight", Game::DP_PARTS));
        p->x = mcDragon->x + hx::random() * ww;
        p->y = mcDragon->y + (hx::random() * 2 - 1) * 10;
        p->vx = hx::random() * ww * 0.75 * sens;
        p->timer = 10 + hx::random() * 20;
    }
    int ma = 10;
    if (x < -ma || x >= Cs::XMAX + ma) kill();
}

void Dragon::kill() {
    mcDragon->removeMovieClip();
    Event::kill();
}

int Storm::TOTAL = 0;

Storm::Storm() {
    Game* g = Game::me;
    TOTAL++;
    root = g->dm->empty(Game::DP_PARTS);
    root->blendMode = flash::Blend::Add;
    Filt::glow(root.get(), 2, 4, 0xFFFF00);
    Filt::glow(root.get(), 14, 2, 0xFFFF00);
    timer = 40;
    g->pad->freeze++;
}

void Storm::update() {
    Game* g = Game::me;
    if (!g->pad) {
        kill();
        return;
    }
    root->clear();
    int max = hx::Int(1 + std::min(timer / 10, 3.0));
    for (int k = 0; k < max; k++) {
        double sx = Cs::getX(bl->x + 0.5), sy = Cs::getY(bl->y + 0.5);
        double ex = g->pad->x + (hx::random() * 2 - 1) * g->pad->ray;
        double ey = g->pad->y;
        int m = 10;
        double ec = 10;
        root->moveTo(sx, sy);
        root->lineStyle(2.5, 0xFFFFFF, 50);
        for (int i = 0; i < m; i++) {
            double c = (i + 1.0) / m;
            double px = sx * (1 - c) + ex * c + (hx::random() * 2 - 1) * ec;
            double py = sy * (1 - c) + ey * c + (hx::random() * 2 - 1) * ec;
            if (i == m - 1) {
                px = ex;
                py = ey;
            }
            root->lineTo(px, py);
        }
    }
    timer -= mt::Timer::tmod;
    if (timer < 0) {
        kill();
        return;
    }
    g->pad->setFlash(2);
}

void Storm::kill() {
    if (dead) return;
    TOTAL--;
    root->removeMovieClip();
    if (Game::me->pad) Game::me->pad->freeze--;
    Event::kill();
}

UltraViolet::UltraViolet() {
    ammo = 30;
    timer = 0;
}

void UltraViolet::update() {
    static const double CYCLE = 4;
    Game* g = Game::me;
    timer -= mt::Timer::tmod;
    if (timer < 0) {
        shoot();
        timer = CYCLE;
        ammo--;
        if (ammo == 0) kill();
    }
    if (hx::random(3) == 0) {
        auto* sp = new fx::Tracer(g->dm->attach("mcUltraRay", Game::DP_BLOCK));
        sp->x = Cs::mcw * 0.5;
        sp->y = Cs::mch + 10;
        sp->root->rotation = 90 - hx::random() * 180;
        sp->root->blendMode = flash::Blend::Add;
        sp->updatePos();
    }
}

void UltraViolet::shoot() {
    Game* g = Game::me;
    std::vector<Block*> list;
    for (int i = 0; i < Cs::YMAX; i++) {
        for (int x = 0; x < Cs::XMAX; x++) {
            int y = Cs::YMAX - (i + 1);
            if (Block* bl = g->gridAt(x, y)) list.push_back(bl);
            if (list.size() > 6) break;
        }
        if (list.size() > 6) break;
    }
    if (!list.empty()) {
        Block* bl = list[hx::random((int)list.size())];
        auto* sp = new fx::Tracer(g->dm->attach("mcUltraViolet", Game::DP_BLOCK));
        sp->x = Cs::mcw * 0.5;
        sp->y = Cs::mch + 10;
        double dx = Cs::getX(bl->x + 0.5) - sp->x, dy = Cs::getY(bl->y + 0.5) - sp->y;
        sp->root->rotation = std::atan2(dy, dx) / 0.0174;
        sp->root->xscale = std::sqrt(dx * dx + dy * dy);
        sp->updatePos();
        bl->explode();
        MC mc = g->dm->attach("mcOndeAnim", Game::DP_PARTS);
        mc->x = Cs::getX(bl->x + 0.5);
        mc->y = Cs::getY(bl->y + 0.5);
        mc->blendMode = flash::Blend::Add;
    }
}

Unification::Unification() {
    ray = 0;
    cx = Cs::XMAX * 0.5;
    cy = 5;
    for (Block* bl : Game::me->blocks) {
        double dx = bl->x - cx, dy = bl->y - cy;
        list.push_back({bl, std::sqrt(dx * dx + dy * dy)});
    }
}

void Unification::update() {
    Game* g = Game::me;
    ray += 0.2 * mt::Timer::tmod;
    auto a = list;
    for (auto& o : a)
        if (o.second < ray) {
            list.erase(std::find(list.begin(), list.end(), o));
            o.first->setType(10);
        }
    int max = hx::Int(2 + Cs::getPerfCoef() * 14);
    double rdec = hx::random();
    for (int i = 0; i < max; i++) {
        double an = (rdec + ((double)i / max - 1)) * 6.28;
        auto* p = new Phys(g->dm->attach("partTwinkle", Game::DP_PARTS));
        p->x = Cs::getX(cx + 0.5 + std::cos(an) * (ray + 1));
        p->y = Cs::getY(cy + 0.5 + std::sin(an) * (ray + 1));
        p->fadeType = 0;
        p->timer = 10 + hx::random() * 10;
    }
    if (list.empty()) kill();
}

Wave::Wave() {
    y = Cs::YMAX - 1;
    mcWave = Game::me->dm->attach("mcWave", Game::DP_PARTS);
}

void Wave::update() {
    struct Anon : Hitter {
        int hType() const override { return 0; }
        double hDamage() const override { return 1; }
    } anon;
    for (int i = 0; i < 2; i++) {
        y--;
        for (int x = 0; x < Cs::XMAX; x++)
            if (Block* bl = Game::me->gridAt(x, y)) bl->damage(&anon);
    }
    mcWave->y = Cs::getY(y);
    if (y < -2) kill();
}

void Wave::kill() {
    mcWave->removeMovieClip();
    Event::kill();
}

// --- Drone
Drone::Drone() {
    Game* g = Game::me;
    root = g->dm->attach("mcDrone", Game::DP_DRONE);
    frame = 0;
    transformSpeed = 0.0022;
    jumpSpeed = 6;
    if (Cs::pi->shopItems.get(ShopInfo::DRONE_PERFO) == 1) transformSpeed = 0.005;
    if (Cs::pi->shopItems.get(ShopInfo::DRONE_SPEED) == 1) jumpSpeed = 18;
    if (Cs::pi->gotItem(MissionInfo::MODE_DIF)) transformSpeed *= 0.33;
    flConverter = Cs::pi->shopItems.get(ShopInfo::DRONE_CONVERTER) == 1;
}

void Drone::update() {
    switch (step) {
        case 0: updateJump(); break;
        case 1: updateBlock(); break;
        case 2: updateCancel(); break;
        default: findTrg();
    }
    if (dead) return;
    if (sparkTimer > 0) {
        sparkTimer -= mt::Timer::tmod;
        int max = hx::Int(sparkTimer * 0.25);
        double ray = 8;
        for (int i = 0; i < max; i++) {
            auto* p = new Phys(Game::me->dm->attach("partSparkTwinkle", Game::DP_PARTS));
            p->x = root->x + (hx::random() * 2 - 1) * ray;
            p->y = root->y + (hx::random() * 2 - 1) * ray;
            p->fadeType = 0;
            p->timer = 10 + hx::random() * 10;
            p->root->gotoAndPlay(1 + hx::random(p->root->totalFrames()));
        }
        if (sparkTimer <= 0) sparkTimer = hx::NUL;
    }
}

void Drone::followPath() {
    double ox = root->x, oy = root->y;
    c = std::min(c + speed * mt::Timer::tmod, 1.0);
    root->x = sx * (1 - c) + ex * c;
    root->y = sy * (1 - c) + ey * c - std::sin(c * 3.14) * (2 / speed);
    MC mc = Game::me->dm->attach("mcQueueDrone", Game::DP_PARTS2);
    mc->x = ox;
    mc->y = oy;
    double dx = root->x - ox, dy = root->y - oy;
    mc->rotation = std::atan2(dy, dx) / 0.0174;
    mc->xscale = std::sqrt(dx * dx + dy * dy);
    root->rotation = std::atan2(dy, dx) / 0.0174;
}

void Drone::setTrg(double tx, double ty) {
    sx = root->x;
    sy = root->y;
    ex = tx;
    ey = ty;
    double dx = ex - sx, dy = ey - sy;
    speed = jumpSpeed / std::sqrt(dx * dx + dy * dy);
}

void Drone::findTrg() {
    bl = getBlock();
    if (!bl) {
        initCancel();
        return;
    }
    c = 0;
    double ty = Cs::getY(bl->y + hx::random());  // args reversed
    double tx = Cs::getX(bl->x + hx::random());
    setTrg(tx, ty);
    step = 0;
    root->stop();
    root->filters.clear();
}

void Drone::updateJump() {
    followPath();
    if (c == 1) initBlock();
}

Block* Drone::getBlock() {
    Block* b = getRandomSentiBlock();
    if (!b && Cs::pi->shopItems.get(ShopInfo::DRONE_COLLECTOR) == 1) b = getRandomBonusBlock();
    return b;
}

Block* Drone::getRandomBonusBlock() {
    std::vector<Block*> list;
    for (int x = 0; x < Cs::XMAX; x++)
        for (int y = 0; y < Cs::YMAX; y++) {
            Block* b = Game::me->gridAt(x, y);
            if (b && Block::isBonus(b->type)) list.push_back(b);
        }
    if (list.empty()) return nullptr;
    return list[hx::random((int)list.size())];
}

Block* Drone::getRandomSentiBlock() {
    std::vector<Block*> list;
    for (int x = 0; x < Cs::XMAX; x++)
        for (int y = 0; y < Cs::YMAX; y++) {
            Block* b = Game::me->gridAt(x, y);
            if (b && Block::isSentinelle(b->type)) list.push_back(b);
        }
    if (list.empty()) return nullptr;
    return list[hx::random((int)list.size())];
}

void Drone::initBlock() {
    step = 1;
    findBlockTrg();
    flash::Filter f;
    f.type = flash::Filter::Shadow;
    f.blurX = f.blurY = 0;
    f.distance = 2;
    f.angle = 45 * 3.14159265 / 180;  // DropShadowFilter default angle
    f.color = 0;
    f.alpha = 0.5;
    root->filters = {f};
}

void Drone::updateBlock() {
    Game* g = Game::me;
    double cc = 0.1;
    double dx = ex - root->x, dy = ey - root->y;
    root->x += dx * cc;
    root->y += dy * cc;
    double move = std::fabs(dx) + std::fabs(dy);
    if (hx::random(hx::Int(14 / mt::Timer::tmod)) == 0 || move < 1) findBlockTrg();
    root->rotation = std::atan2(dy, dx) / 0.0174;
    frame = std::fmod(frame + move * 0.25, 12);
    root->gotoAndStop(hx::Int(frame) + 1);
    if (mt::Timer::tmod > 1.5 && hx::random(3) == 0) {
        auto* p = new fx::Spark(g->dm->attach("partLine", Game::DP_PARTS));
        p->x = root->x;
        p->y = root->y;
        p->vx = (hx::random() * 2 - 1) * 2;
        p->vy = -hx::random() * 1.5;
        p->weight = 0.1 + hx::random() * 0.1;
        p->timer = 10 + hx::random() * 10;
        p->coef = 2;
        p->root->yscale = 150;
        Filt::glow(p->root.get(), 10, 3, 0xFFFF00);
        p->root->blendMode = flash::Blend::Add;
    }
    int become = 0;
    if (flConverter) become = Block::BONUS;
    if (bl->type != Block::LURE) bl->incTransform(transformSpeed * mt::Timer::tmod, become);
    if (bl->transform == 1 || bl->flDeath || (!Block::isSentinelle(bl->type) && !Block::isBonus(bl->type))) {
        bl->transform = 0;
        if (g->pad) {
            auto drones = g->pad->drones;
            for (auto* d : drones)
                if (d->bl == bl) d->findTrg();
        }
    }
}

void Drone::findBlockTrg() {
    ex = Cs::getX(bl->x + hx::random());
    ey = Cs::getY(bl->y + hx::random());
}

void Drone::initCancel() {
    step = 2;
    c = 0;
    jumpSpeed = 16;
    setTrg(Game::me->pad->x, Game::me->pad->y);
}

void Drone::updateCancel() {
    Pad* pad = Game::me->pad;
    ex = pad->x;
    ey = pad->y;
    followPath();
    if (c == 1) {
        hx::remove(pad->drones, this);
        kill();
    }
}

void Drone::explode() {
    Game* g = Game::me;
    MC mc = g->dm->attach("partExplode", Game::DP_PARTS);
    mc->x = root->x - Cs::BW * 0.5;
    mc->y = root->y - Cs::BH * 0.5;
    Col::setColor(mc.get(), 0xFCF0B0, -220);
    MC m2 = g->dm->attach("fxDroneMiniExplosion", Game::DP_PARTS);
    m2->x = root->x;
    m2->y = root->y;
    m2->xscale = m2->yscale = 100 + hx::random() * 100;
    m2->gotoAndPlay(hx::random(3) + 1);
    m2->blendMode = flash::Blend::Add;
    kill();
}

void Drone::kill() {
    root->removeMovieClip();
    Event::kill();
}

}  // namespace ev

// ------------------------------------------------------------------ fx

namespace fx {

void Attract::update() {
    Pad* pad = Game::me->pad;
    if (!pad) {
        kill();
        return;
    }
    double ox = x, oy = y;
    double tx = pad->x + dx, ty = pad->y + dy;
    toward(tx, ty, 0.3);
    double vx_ = x - ox, vy_ = y - oy;
    double vit = std::sqrt(vx_ * vx_ + vy_ * vy_);
    double a = std::atan2(vy_, vx_);
    root->xscale = vit;
    root->rotation = a / 0.0174;
    Phys::update();
    if (getDist(tx, ty) < 6) kill();
}

Fly::Fly() : Phys(Game::me->dm->attach("mcInsect", Game::DP_BALL)) {
    angle = hx::random() * 6.28;
    frict = 0.95;
    speed = 7 + hx::random() * 7;
    x = hx::random() * Cs::mcw;
    y = Cs::mch + 10;
    life = hx::random(1000);
    step = 0;
    root->stop();
    updatePos();
}

void Fly::update() {
    Pad* pad = Game::me->pad;
    if (!pad) {
        kill();
        return;
    }
    switch (step) {
        case 0: {
            if (hx::isNull(dx)) {
                dx = (hx::random() * 2 - 1) * (pad->ray + 30);
                dy = -hx::random() * 50 + 20;
            }
            double ddx = pad->x + dx - x, ddy = pad->y + dy - y;
            double ta = std::atan2(ddy, ddx);
            double dist = std::sqrt(ddx * ddx + ddy * ddy);
            double sp = std::min(speed, dist * 0.1);
            double da = Num::hMod(ta - angle, 3.14);
            angle = Num::hMod(angle + da * 0.2, 3.14);
            x += std::cos(angle) * sp;
            y += std::sin(angle) * sp;
            bool flGo = hx::random() / mt::Timer::tmod < 0.05;
            if (dist < 20 || flGo) dx = hx::NUL;
            if (std::fabs(pad->x - x) < pad->ray && std::fabs(pad->y + 6 - y) < 6) {
                life += 20 * mt::Timer::tmod;
                if (life > 1000) {
                    step = 1;
                    dx = x - pad->x;
                    dy = y - pad->y;
                    life = 1000;
                    ox = Cs::MX;
                    vx = 0;
                    vy = 0;
                    pad->insect++;
                    root->play();
                }
            }
            break;
        }
        case 1:
            x = pad->x + dx;
            y = pad->y + dy;
            life -= std::fabs(ox - Cs::MX) * 0.4;
            ox = Cs::MX;
            if (life <= 0) {
                vx = (hx::random() * 2 - 1) * 30;
                vy = -(5 + hx::random() * 20);
                step = 0;
                dx = hx::NUL;
                pad->insect--;
                root->gotoAndStop(1);
            }
            break;
    }
    Phys::update();
    if (auto s = root->smc()) s->gotoAndStop(2);
    Game::me->plasmaDraw(root.get());
    if (auto s = root->smc()) s->gotoAndStop(1);
}

Jumper::Jumper(MC mc) : Phys(std::move(mc)) {
    Filt::glow(root.get(), 2, 4, 0xFFFFFF);
    Filt::glow(root.get(), 10, 2, 0xFFFFFF);
}

void Jumper::update() {
    Game* g = Game::me;
    double ox = x, oy = y;
    c = std::min(c + 0.1 * mt::Timer::tmod, 1.0);
    x = sx * (1 - c) + ex * c;
    y = sy * (1 - c) + ey * c - std::sin(c * 3.14) * 20;
    Phys::update();
    MC mc = g->bdm->attach("mcQueueJumper", 0);
    mc->x = ox;
    mc->y = oy;
    double dx = x - ox, dy = y - oy;
    mc->rotation = std::atan2(dy, dx) / 0.0174;
    mc->xscale = std::sqrt(dx * dx + dy * dy);
    if (c == 1) {
        if (g->gridAt(tx, ty)) {
            seekTrg();
        } else {
            bl->setPos(tx, ty);
            bl->register_();
            bl->root->visible = true;
            g->bdm->over(bl->root);
            kill();
        }
    }
}

void Jumper::seekTrg() {
    Game* g = Game::me;
    tx = -1;
    int to = 0;
    while (to++ < 200) {
        int rx = hx::random(Cs::XMAX);
        int ry = hx::random(g->level->ymax);
        if (!g->gridAt(rx, ry)) {
            tx = rx;
            ty = ry;
            break;
        }
    }
    if (tx < 0) {
        bl->root->removeMovieClip();
        kill();
        return;
    }
    sx = x;
    sy = y;
    ex = Cs::getX(tx + 0.5);
    ey = Cs::getY(ty + 0.5);
    c = 0;
}

void LineUp::update() {
    Phys::update();
    double xs = -100 * factor * vx, ys = -100 * factor * vy;
    if (std::fabs(xs) < 100) xs = 100;
    if (std::fabs(ys) < 100) ys = 100;
    root->xscale = xs;
    root->yscale = ys;
}

Part::Part(MC mc) : Phys(std::move(mc)) { bouncer = std::make_unique<Bouncer>(this); }

void Part::update() {
    Phys::update();
    x -= vx * mt::Timer::tmod;
    y -= vy * mt::Timer::tmod;
    bouncer->update();
    updatePos();
}

void Spark::update() {
    double vit = std::sqrt(vx * vx + vy * vy);
    double a = std::atan2(vy, vx);
    root->xscale = vit * coef;
    root->rotation = a / 0.0174;
    Phys::update();
}

void Tracer::update() {
    Phys::update();
    Game::me->plasmaDraw(root.get());
}

}  // namespace fx
