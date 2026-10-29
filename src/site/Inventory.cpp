#include "Inventory.h"

#include <algorithm>
#include <cmath>

#include "flash/Stage.h"
#include "game/Game.h"
#include "mt/DepthManager.h"
#include "mt/Random.h"

namespace site {

namespace Col = mt::Col;
namespace Filt = mt::Filt;

static constexpr int GREY = 0x384561;  // locked items are drawn as dark silhouettes

Inventory::Inventory() = default;
Inventory::~Inventory() = default;

bool Inventory::init(const std::string& assetsDir, const std::string& dataDir, const std::string& lang) {
    auto* st = flash::Stage::get();
    auto it = st->libraries.find("inv");
    lib = it != st->libraries.end() ? it->second.get() : st->loadLibrary(assetsDir, "inv");
    if (!lib) return false;
    auto pl = st->libraries.find("planet");
    planetLib = pl != st->libraries.end() ? pl->second.get() : nullptr;
    size_t sz = 0;
    void* raw = SDL_LoadFile((dataDir + "/inv_" + lang + ".json").c_str(), &sz);
    if (!raw) raw = SDL_LoadFile((dataDir + "/inv_en.json").c_str(), &sz);
    if (raw) {
        try {
            txt = nlohmann::json::parse((const char*)raw, (const char*)raw + sz);
        } catch (...) {
        }
        SDL_free(raw);
    }
    return true;
}

// ------------------------------------------------------------------ texts

std::string Inventory::fmt(const nlohmann::json& nd, const std::vector<int>& args) const {
    std::string name, desc;
    if (nd.is_array() && nd.size() >= 2) {
        name = nd[0].get<std::string>();
        desc = nd[1].get<std::string>();
    }
    for (size_t i = 0; i < args.size(); i++) {
        std::string key = "$" + std::to_string(i), val = std::to_string(args[i]);
        for (size_t p; (p = desc.find(key)) != std::string::npos;) desc.replace(p, key.size(), val);
    }
    return "<b>" + name + "</b>\n<font size='9'>" + desc + "</font>";
}

std::string Inventory::item(int i, const std::vector<int>& args) const {
    const auto& a = txt["items"];
    return fmt(i >= 0 && i < (int)a.size() ? a[i] : nlohmann::json(), args);
}

std::string Inventory::misc(int i, const std::vector<int>& args) const {
    const auto& a = txt["misc"];
    return fmt(i >= 0 && i < (int)a.size() ? a[i] : nlohmann::json(), args);
}

void Inventory::grey(const flash::MC& mc) { Col::setPercentColor(mc.get(), 100, GREY); }

// ------------------------------------------------------------------ build

void Inventory::build() {
    if (!lib || !Cs::pi) return;
    hints.clear();
    bitmaps.clear();
    current = -1;
    root = std::make_shared<flash::MovieClip>(lib, nullptr);
    root->constructed = true;
    dm = std::make_unique<mt::DepthManager>(root);

    // background: mcBg + the panel frames + shading, flattened into one bitmap
    bg = std::make_shared<flash::BitmapData>(flash::Stage::get(), (int)W, (int)H, true, 0);
    MC holder = dm->empty(0);
    holder->attachBitmap(bg, 0);
    for (const char* n : {"mcBg", "cadres", "mcShade"}) {
        MC mc = dm->attach(n, 0);
        bg->draw(mc.get(), flash::Matrix());
        mc->removeMovieClip();
    }

    initPad();
    initDrones();
    initMissiles();
    initStars();
    initCircle();
    initPlanets();
    initGadgets();
    initEarthMap();
    initPassengers();

    PlayerInfo* pi = Cs::pi;
    // BALLS
    int balls = 0;
    if (pi->gotItem(MissionInfo::BALL_DRILL)) balls = 1;
    if (pi->gotItem(MissionInfo::BALL_SOLDAT)) balls = 2;
    if (pi->gotItem(MissionInfo::BALL_POWER)) balls = 3;
    if (pi->gotItem(MissionInfo::BALL_BLACK)) balls = 4;
    for (int i = 0; i < 4; i++) {
        MC mc = dm->attach("mcBall", 1);
        mc->setX(13 + 15 * (i % 2));
        mc->setY(53 + 15 * (i / 2));
        mc->setXScale(80);
        mc->setYScale(80);
        if (i < balls) {
            mc->gotoAndStop(i + 1);
            setHint(mc, item(i));
        } else {
            mc->gotoAndStop(10);
        }
    }
    // MAPS (shops, missiles; the black hole map is never given)
    bool maps[3] = {pi->gotItem(MissionInfo::MAP_SHOP), pi->shopItems.get(ShopInfo::MISSILE_MAP) == 1, false};
    for (int i = 0; i < 3; i++) {
        MC mc = dm->attach("mcMap", 1);
        mc->setX(17 + 43 * i);
        mc->setY(90);
        int fr = 1;
        if (maps[i]) {
            fr = 2 + i;
            setHint(mc, item(12 + i));
        }
        mc->gotoAndStop(fr);
    }
    // GENERATORS: engine, radar, surface reactor
    MC eng = dm->attach("mcGenerator", 1);
    eng->setX(130);
    eng->setY(20);
    if (auto s = eng->smc()) s->stop();
    if (pi->engine <= 1) {
        grey(eng);
    } else {
        if (auto f = eng->field("+Da3+")) f->setText(std::to_string(pi->engine - 1));
        setHint(eng, misc(0, {pi->engine}));
    }
    MC rad = dm->attach("mcGenerator", 1);
    rad->setX(60);
    rad->setY(59);
    if (auto s = rad->smc()) s->gotoAndStop(3);
    if (pi->radar <= 1) {
        grey(rad);
    } else {
        if (auto f = rad->field("+Da3+")) f->setText(std::to_string(pi->radar - 1));
        setHint(rad, misc(11, {pi->radar}));
    }
    MC lan = dm->attach("mcGenerator", 1);
    lan->setX(130);
    lan->setY(59);
    if (auto s = lan->smc()) s->gotoAndStop(2);
    if (!pi->gotItem(MissionInfo::LANDER_REACTOR)) {
        grey(lan);
    } else {
        int n = 1;
        for (int i = 0; i < 3; i++)
            if (pi->shopItems.get(ShopInfo::LANDER_REACTOR_0 + i) == 1) n++;
        if (auto f = lan->field("+Da3+")) f->setText(std::to_string(n));
        setHint(lan, misc(9, {n}));
    }
    MC pod = dm->attach("mcPod", 1);
    pod->setX(99);
    pod->setY(60);
    if (pi->shopItems.get(ShopInfo::PODS) != 1) {
        grey(pod);
        pod->stop();
    } else {
        int n = 1;
        for (int i = 0; i < 3; i++)
            if (pi->shopItems.get(ShopInfo::PODS_EXTEND_0 + i) == 1) n++;
        setHint(pod, misc(10, {n * 2}));
        pod->gotoAndStop(n);
    }

    hint = dm->attach("mcHint", 10);
    hint->setAlpha(0);
    hintOn = false;
}

void Inventory::initPad() {
    PlayerInfo* pi = Cs::pi;
    double ray = 32;
    if (pi->gotItem(MissionInfo::EXTENSION)) ray += 10;
    MC pad = dm->attach("mcPad", 1);
    MC mid = pad->clip("-]A"), left = pad->clip("}u_q5"), right = pad->clip(" v_q5");
    double w = ray - 12;
    if (mid) {
        mid->setXScale(w * 2);
        mid->setX(-w);
        mid->stop();
    }
    if (left) {
        left->setX(-ray);
        left->stop();
    }
    if (right) {
        right->setX(ray);
        right->stop();
    }
    int fr = pi->gotItem(MissionInfo::MEDAL) ? 2 : 1;
    for (auto& c : {mid, left, right})
        if (c && c->smc()) c->smc()->gotoAndStop(fr);
    pad->setX(55);
    pad->setY(5);
    // spare envelopes
    int life = pi->getLife();
    for (int i = 0; i < 4; i++) {
        MC mc = dm->attach("mcMiniPad", 1);
        mc->setX(18 + i * 25);
        mc->setY(30);
        if (i < life) {
            std::string s = std::to_string(life) + (life == 1 ? txt.value("life", "") : txt.value("lives", ""));
            setHint(mc, s);
        } else {
            grey(mc);
        }
    }
}

void Inventory::initDrones() {
    PlayerInfo* pi = Cs::pi;
    MC d = dm->attach("mcDrone", 1);
    d->setX(47);
    d->setY(163);
    d->setXScale(77);
    d->setYScale(77);
    if (pi->drone != 0) {
        d->gotoAndStop(2);
        setHint(d, misc(1));
    } else {
        d->stop();
    }
    for (int i = 0; i < 10; i++) {
        MC mc = dm->attach("mcDrone", 1);
        mc->setX(15 + 16 * (i % 5));
        mc->setY(197 + 15 * (i / 5));
        mc->setXScale(21);
        mc->setYScale(21);
        mc->gotoAndStop(i < pi->drone ? 2 : 1);
    }
    const int caps[4] = {ShopInfo::DRONE_PERFO, ShopInfo::DRONE_SPEED, ShopInfo::DRONE_CONVERTER,
                         ShopInfo::DRONE_COLLECTOR};
    for (int i = 0; i < 4; i++) {
        MC mc = dm->attach("mcDroneCapsule", 1);
        mc->setX(108);
        mc->setY(140 + 20 * i);
        int fr = i * 2 + 1;
        if (pi->shopItems.get(caps[i]) == 1) {
            fr++;
            setHint(mc, item(8 + i));
        }
        mc->gotoAndStop(fr);
    }
}

void Inventory::initMissiles() {
    PlayerInfo* pi = Cs::pi;
    int type = 0;
    if (pi->missileMax > 0) type = 1;
    if (pi->gotItem(MissionInfo::MISSILE_BLUE)) type = 2;
    if (pi->gotItem(MissionInfo::MISSILE_BLACK)) type = 3;
    if (pi->gotItem(MissionInfo::MISSILE_RED)) type = 4;
    for (int i = 0; i < 4; i++) {
        MC mc = dm->attach("mcMisHead", 1);
        mc->setX(326 + (i % 2) * 15);
        mc->setY(18 + (i / 2) * 16);
        mc->setXScale(50);
        mc->setYScale(50);
        int fr = 1;
        if (i < type) {
            setHint(mc, item(4 + i));
            fr += i + 1;
        }
        mc->gotoAndStop(fr);
    }
    const int opts[3] = {ShopInfo::LATERAL, ShopInfo::COOLER, ShopInfo::MISSILE_GENERATOR};
    for (int i = 0; i < 3; i++) {
        MC mc = dm->attach("mcMissileOpt", 1);
        mc->setX(348 + i * 35);
        mc->setY(6);
        mc->gotoAndStop(i + 1);
        mc->setXScale(85);
        mc->setYScale(85);
        if (pi->shopItems.get(opts[i]) == 1) setHint(mc, item(18 + i));
        else grey(mc);
    }
    for (int i = 0; i < 49; i++) {
        MC mc = dm->attach("mcMissile", 1);
        mc->setX(323 + (i % 7) * 8);
        mc->setY(61 + (i / 7) * 11);
        mc->setXScale(60);
        mc->setYScale(60);
        int fr = 1;
        if (i < pi->missileMax) {
            Filt::glow(mc.get(), 2, 2, 0x44);
            fr += type;
            if (!(i < pi->missile)) mc->setAlpha(20);
        }
        mc->gotoAndStop(fr);
    }
}

void Inventory::initStars() {
    PlayerInfo* pi = Cs::pi;
    for (int i = 0; i < 7; i++) {
        MC mc = dm->attach("mcStar", 1);
        double x = 270, y = 192, dx = 24, dy = 17;
        if (i < 2) {
            x += (i * 2 - 1) * dx * 0.5;
            y -= dy;
        } else if (i < 5) {
            x += (i - 3) * dx;
        } else {
            x += ((i - 5) * 2 - 1) * dx * 0.5;
            y += dy;
        }
        mc->setX(x);
        mc->setY(y);
        int fr = 1;
        if (pi->gotItem(MissionInfo::STAR_RED + i)) {
            fr += i + 1;
            const auto& s = txt["stars"];
            if (i < (int)s.size()) setHint(mc, s[i].get<std::string>());
        }
        mc->gotoAndStop(fr);
    }
}

void Inventory::initCircle() {
    PlayerInfo* pi = Cs::pi;
    double cx = 230, cy = 81;
    // the three licences
    for (int i = 0; i < 3; i++) {
        double a = (i / 3.0) * 6.28 - 1.57 + 0.04, r = 30.36;
        MC mc = dm->attach("mcCirclePass", 1);
        mc->setX(cx + std::cos(a) * r);
        mc->setY(cy + std::sin(a) * r);
        mc->setXScale(60);
        mc->setYScale(60);
        int fr = 1;
        if (pi->gotItem(MissionInfo::CARD_RED + i)) {
            fr += 1 + i;
            setHint(mc, item(15 + i));
        }
        mc->gotoAndStop(fr);
    }
    // the twelve Karbonite tablets around them
    for (int i = 0; i < 12; i++) {
        double a = ((i - 0.5) / 12) * 6.28, r = 60;
        MC mc = dm->attach("mcTablet", 1);
        mc->setX(cx + std::cos(a) * r);
        mc->setY(cy + std::sin(a) * r);
        int fr = 1;
        if (pi->gotItem(MissionInfo::TBL_0 + i)) {
            fr++;
            setHint(mc, misc(2));
        }
        mc->gotoAndStop(fr);
        if (auto s = mc->smc()) s->gotoAndStop(i + 1);
    }
}

void Inventory::initPlanets() {
    PlayerInfo* pi = Cs::pi;
    int col = 0, side = -1;
    double y = 244;
    for (int id = 0; id < 24; id++) {
        if (id == ZoneInfo::ASTEROBELT) continue;
        MC mc = dm->attach("mcPlanet", 1);
        mc->setX(36 + side * 14.5 + col * 59.5);
        mc->setY(y);
        col++;
        if (id == 7 || id == 15) {
            y += 27.5;
            side = -side;
            col = 0;
        }
        mc->setXScale(35);
        mc->setYScale(35);
        int pct = id < (int)pi->comp.size() ? pi->comp[id] : 0;
        mc->gotoAndStop(hx::Int((1 - pct / 100.0) * 160) + 1);
        if (auto f = mc->field("+Da3+")) f->setText(std::to_string(pct) + "%");
        // planet.swf is loaded into the clip, showing the planet without its ring of zones
        // (at 0% the shading covers it all; skipping it avoids hairline seams between the shading quarters)
        if (MC holder = mc->smc(); holder && planetLib && pct > 0) {
            holder->loadRoot(planetLib);
            if (MC p = holder->smc()) {
                p->gotoAndStop(id + 1);
                if (MC s = p->smc()) s->visible = false;
            }
        }
        if (pct > 0) {
            std::string desc = txt.value("exploring", "");
            if (pct == 100 && id < (int)txt["planets"].size()) desc = txt["planets"][id].get<std::string>();
            std::string name = id < (int)ZoneInfo::list.size() ? ZoneInfo::list[id].name : "";
            setHint(mc, "<b>" + name + "</b>\n<font size='9'>" + desc + "</font>");
        }
    }
}

void Inventory::initGadgets() {
    PlayerInfo* pi = Cs::pi;
    // positive: shop item, negative: mission item, 0: always (the slot works out what to show)
    const int list[13] = {ShopInfo::SUNGLASSES, 0,     ShopInfo::ANTENNA, 0,   -MissionInfo::SUPER_RADAR,
                          -MissionInfo::SYNTROGEN, -MissionInfo::RETROFUSER, -MissionInfo::COMBINAISON,
                          -MissionInfo::MINES, 0, -MissionInfo::GENERATOR, 0, -MissionInfo::BALL_DOUBLE};
    for (int i = 0; i < 15; i++) {
        MC slot = dm->attach("mcGadgetSlot", 1);
        slot->setX(316 + (i % 5) * 28);
        slot->setY(148 + (i / 5) * 25);
        if (i >= 13) continue;
        int id = list[i];
        bool show = (id > 0 && pi->shopItems.get(id) == 1) || id == 0 || (id < 0 && pi->gotItem(-id));
        if (!show) continue;
        MC g = dm->attach("mcGadget", 1);
        g->setX(slot->x);
        g->setY(slot->y);
        g->gotoAndStop(i + 1);
        Filt::glow(g.get(), 2, 2, 0xFFFFFF);
        MC s = g->smc();
        std::string h = item(21 + i);
        switch (i) {
            case 1: {  // the medallion, or its pieces
                if (!pi->gotItem(MissionInfo::MEDAL)) {
                    if (s) s->gotoAndStop(1);
                    bool any = false;
                    for (int k = 0; k < 3; k++) {
                        MC p = s ? s->clip("_p" + std::to_string(k)) : nullptr;
                        bool v = pi->gotItem(MissionInfo::MEDAL_0 + k);
                        if (p) p->visible = v;
                        any |= v;
                    }
                    if (any) setHint(slot, item(41));
                } else {
                    if (s) s->gotoAndStop(2);
                    setHint(slot, h);
                }
                break;
            }
            case 3: {  // antimatter nuclei
                int n = 0;
                for (int k = 0; k < 4; k++)
                    if (pi->gotItem(MissionInfo::ANTIMAT_0 + k)) n++;
                if (n) {
                    if (s) s->gotoAndStop(n);
                    setHint(slot, misc(3, {n}));
                } else {
                    if (s) s->stop();
                    g->visible = false;
                }
                break;
            }
            case 8: {  // FORA mines
                int n = 1;
                for (int k = 0; k < 3; k++)
                    if (pi->shopItems.get(ShopInfo::MINE_0 + k) == 1) n++;
                setHint(slot, misc(8, {n}));
                if (s) s->gotoAndStop(n);
                break;
            }
            case 9: {  // rock samples
                int n = 0;
                if (pi->gotItem(MissionInfo::STONE_LYCANS)) n += 1;
                if (pi->gotItem(MissionInfo::STONE_SPIGNYSOS)) n += 2;
                if (n) {
                    if (s) s->gotoAndStop(n);
                    setHint(slot, misc(n == 1 ? 5 : n == 2 ? 6 : 7));
                } else {
                    if (s) s->stop();
                    g->visible = false;
                }
                break;
            }
            case 11: {  // pink crystals
                int n = 0;
                for (int k = 0; k < 5; k++)
                    if (pi->gotItem(MissionInfo::CRYSTAL_0 + k)) n++;
                if (n) {
                    if (s) s->gotoAndStop(n);
                    setHint(slot, misc(4, {n}));
                } else {
                    if (s) s->stop();
                    g->visible = false;
                }
                break;
            }
            default: setHint(slot, h);
        }
    }
}

void Inventory::initEarthMap() {
    PlayerInfo* pi = Cs::pi;
    MC map = dm->attach("mcEarthMap", 1);
    map->setX(162);
    map->setY(169);
    map->gotoAndStop(1);
    const int W7 = 7, H6 = 6, SQ = 8;
    bool got[7][6] = {};
    int count = 0;
    for (int i = 0; i < W7 * H6; i++) {
        bool g = pi->gotItem(MissionInfo::EMAP_0 + i);
        got[i % W7][i / W7] = g;
        if (g) count++;
    }
    if (count == 0) return;
    int total = W7 * H6;
    bool complete = count == total;
    // the pieces are scattered over the map with a fixed shuffle
    mt::Rand seed(456);
    for (int i = 0; i < 100; i++) {
        int x1 = seed.random(W7), y1 = seed.random(H6), x2 = seed.random(W7), y2 = seed.random(H6);
        std::swap(got[x1][y1], got[x2][y2]);
    }
    map->gotoAndStop(2);
    // Earth's coordinates (shown once the map is complete), placed from the player id like the map does
    mt::Rand es(pi->pid);
    int ray = 1500 + es.random(500);
    int a = es.random(ray);
    int ex = a * (es.random(2) * 2 - 1);
    int ey = (ray - std::abs(ex)) * (es.random(2) * 2 - 1);
    if (auto f = map->field("+Da3+")) {
        f->visible = complete;
        f->setText("[" + std::to_string(ex) + "]");
    }
    if (auto f = map->field(" bXX(")) {
        f->visible = complete;
        f->setText("[" + std::to_string(ey) + "]");
    }
    auto* st = flash::Stage::get();
    auto full = std::make_shared<flash::BitmapData>(st, W7 * SQ, H6 * SQ, true, 0);
    auto part = std::make_shared<flash::BitmapData>(st, W7 * SQ, H6 * SQ, true, 0);
    full->draw(map.get(), flash::Matrix());
    // copyPixels of the squares you own: the full map minus the missing squares
    part->draw(full.get(), flash::Matrix());
    for (int x = 0; x < W7; x++)
        for (int y = 0; y < H6; y++)
            if (!got[x][y]) {
                flash::Rect r;
                r.add(x * SQ, y * SQ);
                r.add(x * SQ + SQ, y * SQ + SQ);
                part->fillRect(r, 0);
            }
    MC a1 = dm->empty(1);
    a1->attachBitmap(full, 0);
    a1->setX(map->x);
    a1->setY(map->y);
    a1->setAlpha(20);
    MC a2 = dm->empty(1);
    a2->attachBitmap(part, 0);
    a2->setX(map->x);
    a2->setY(map->y);
    Filt::glow(a2.get(), 2, 4, 0x00FF00);
    if (count >= total) setHint(a2, misc(13));
    else setHint(a2, misc(12, {total - count}));
    bitmaps.push_back(full);
    bitmaps.push_back(part);
    map->gotoAndStop(1);
}

void Inventory::initPassengers() {
    PlayerInfo* pi = Cs::pi;
    std::vector<std::string> names;
    if (pi->gotItem(MissionInfo::DOUGLAS)) names.push_back("Douglas");
    if (pi->gotItem(MissionInfo::SALMEEN)) names.push_back("Salmeen");
    for (auto& t : pi->travel) names.push_back(t.name);
    int n = 0;
    for (auto& name : names) {
        MC mc = dm->attach("mcPassenger", 1);
        if (auto f = mc->field("+Da3+")) f->setText(name);
        mc->setX(394);
        mc->setY(55 + n * 13);
        n++;
        if (n == 6) return;  // sic: the sixth gets no glow
        Filt::glow(mc.get(), 2, 4, 0x283541);
    }
}

// ------------------------------------------------------------------ hints

void Inventory::showHint(const std::string& html) {
    if (!hint) return;
    auto f = hint->field("+Da3+");
    MC back = hint->clip(";+");
    if (f) {
        f->setWidth(150);
        f->setHtmlText(html);
        f->layout();
        f->setWidth(f->textWidth + 5);
        f->setHeight(f->textHeight + 5);
        f->layout();
        f->setX(1);
        f->setY(1);
    }
    if (back && f) {
        back->setXScale(f->textWidth + 8);
        back->setYScale(f->textHeight + 6);
    }
    hintOn = true;
}

void Inventory::pointer(double ix, double iy, bool inside) {
    mx = ix;
    my = iy;
    int found = -1;
    if (inside && root) {
        for (int i = (int)hints.size() - 1; i >= 0; i--) {
            auto& h = hints[i];
            if (!h.mc || h.mc->removed || !h.mc->visible) continue;
            flash::Rect r = h.mc->getBounds(root.get());
            if (ix >= r.xMin && ix <= r.xMax && iy >= r.yMin && iy <= r.yMax) {
                found = i;
                break;
            }
        }
    }
    if (found != current) {
        current = found;
        if (found >= 0) showHint(hints[found].html);
        else hintOn = false;
    }
}

void Inventory::update() {
    if (!hint) return;
    if (hintOn) hint->setAlpha(std::min(hint->alpha + 20, 100.0));
    else hint->setAlpha(std::max(hint->alpha - 20, 0.0));
    auto f = hint->field("+Da3+");
    double dx = 10, dy = 24;
    if (mx > W * 0.5 && f) dx = -(10 + f->width_);
    if (my > H * 0.7 && f) dy = -f->height_;
    double c = 0.5;
    hint->setX(hint->x + ((mx + dx) - hint->x) * c);
    hint->setY(hint->y + ((my + dy) - hint->y) * c);
}

void Inventory::render(double x, double y, double scale) {
    if (!root) return;
    auto* st = flash::Stage::get();
    st->renderObject(root.get(), flash::Matrix::make(scale, 0, 0, scale, x, y), flash::ColorTransform(),
                     flash::Blend::Normal, false);
}

}  // namespace site
