#include "Map.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "Shell.h"
#include "mt/Bouille.h"

namespace Col = mt::Col;
namespace Filt = mt::Filt;
namespace Num = mt::Num;

namespace navi {

int Map::SX = 0;
int Map::SY = 0;
Map* Map::me = nullptr;

// ================================================================== Map

Map::Map(MC mc) : root(std::move(mc)) {
    me = this;
    dm = std::make_unique<mt::DepthManager>(root);
    flView = true;
}

Map::~Map() {
    menu.reset();
    deadMenu.reset();
    game.reset();
    if (me == this) me = nullptr;
}

void Map::init() {
    PlayerInfo* pi = Cs::pi;
    if (map) {
        // re-initialised without going through a level (test --play): drop the previous map first
        flActive = false;
        unactive();
        removeIcons();
        cleanAll();
    }
    flFuel = pi->chs + pi->chl > 0;
    SX = pi->x - hx_;
    SY = pi->y - hy_;

    // SEED TABLE
    const int TW = XMAX + ZONE_MARGIN * 2, TH = YMAX + ZONE_MARGIN * 2;
    seedTable.clear();
    seedTable.resize(TW);
    for (int x = 0; x < TW; x++) {
        seedTable[x].resize(TH);
        for (int y = 0; y < TH; y++) {
            int px = x + SX - ZONE_MARGIN, py = y + SY - ZONE_MARGIN;
            seedTable[x][y] = std::make_unique<mt::Random>(hx::Int((double)px * (1000 + py) + py));
        }
    }

    initZones();
    initMap();

    if (!layer) {
        zoomCoef = 1;
        step = Zoom;
        zx = hx_;
        zy = hy_;
        zsens = -1;
    } else {
        step = Hole;
        holeStep = 0;
    }
}

void Map::update() {
    deadMenu.reset();
    if (pendingMove) {
        pendingMove = false;
        confirmMove(pendingX, pendingY, true);
    }
    switch (step) {
        case Move: updateMove(); break;
        case Zoom: updateZoom(); break;
        case Hole: updateHole(); break;
        case Land: updateLand(); break;
        default: break;
    }
    updateIcons();
    updateMenu();
    if (game) game->update();
}

// ------------------------------------------------------------------ MOVE

void Map::initMove() {
    menus.clear();
    initIcons();
    initMenu();
    flActive = true;
    switchView(true);
}

void Map::updateMove() {
    if (Cs::PREF_BOOLS[1] && mcMoveZone) {
        moveZoneDec = std::fmod(moveZoneDec + 23 * mt::Timer::tmod, 628);
        mcMoveZone->setAlpha(30 + std::cos(moveZoneDec * 0.01) * 45);
    }
    updateCoord();
    if (heroMove) updateHero();
}

void Map::updateCoord() {
    P p = getMouseSector();
    double tx = ((p.x + 0.5) - SX) * BW;
    double ty = ((p.y + 0.5) - SY) * BH;
    if (!mcCoord) {
        mcCoord = dm->attach("mcCoord", DP_COORD);
        mcCoord->setX(tx);
        mcCoord->setY(ty);
        mcCoord->cacheAsBitmap = true;
    }
    mcCoord->filters.clear();
    PlayerInfo* pi = Cs::pi;
    bool flFarWarning = false;
    auto rc = reach.find({p.x - SX, p.y - SY});
    if (rc != reach.end() && rc->second) {
        if (mcCoord->currentFrame() == 2) mcCoord->gotoAndStop(1);
        if (!pi->gotItem(MissionInfo::BALL_DRILL) && std::max(std::abs(p.x), std::abs(p.y)) >= 3) {
            flFarWarning = true;
            if (!mcFarWarning) mcFarWarning = dm->attach("mcFarZone", DP_INTER);
            mcFarWarning->gotoAndStop(2);
            if (auto f = mcFarWarning->field("field")) f->setText(Text::str("WARNING_CARDS"));
        }
    } else {
        if (mcCoord->currentFrame() == 1) mcCoord->gotoAndStop(2);
        if (std::abs(pi->x) + std::abs(pi->y) <= 3) {
            flFarWarning = true;
            if (!mcFarWarning && pi->missions.get(0) != 0) mcFarWarning = dm->attach("mcFarZone", DP_INTER);
            if (mcFarWarning) {
                mcFarWarning->gotoAndStop(1);
                if (auto f = mcFarWarning->field("field")) f->setText(Text::str("WARNING_FAR"));
            }
        }
    }
    if (!flFarWarning && mcFarWarning) {
        mcFarWarning->removeMovieClip();
        mcFarWarning = nullptr;
    }
    if (auto f = mcCoord->field("field")) f->setText("[" + std::to_string(p.x) + "][" + std::to_string(p.y) + "]");
    if (auto f = mcCoord->field("field2")) f->setText(getSquareName(p.x - SX, p.y - SY));
    double c = 0.5;
    mcCoord->setX(mcCoord->x + (tx - mcCoord->x) * c);
    mcCoord->setY(mcCoord->y + (ty - mcCoord->y) * c);
}

std::string Map::getSquareName(int x, int y) const {
    if (!inGrid(x, y)) return "";
    int id = zoneTable[x][y];
    if (id < 0 || id >= (int)ZoneInfo::list.size()) return "";
    std::string n = ZoneInfo::list[id].name;
    for (auto& ch : n) ch = (char)std::toupper((unsigned char)ch);
    return n;
}

// ------------------------------------------------------------------ ZOOM

void Map::startZoom(int x, int y, bool flMinerai) {
    flActive = false;
    zoomCoef = 0;
    step = Zoom;
    zx = x;
    zy = y;
    zsens = 1;
    int wx = x + SX, wy = y + SY;
    game->initLevel(wx, wy, inGrid(x, y) ? zoneTable[x][y] : -1, flMinerai, nullptr);

    // SCREEN: a snapshot of the level, zoomed into from its square
    mcScreenshot = mdm->empty(DP_ICONS);
    screenshotBmp = std::make_shared<flash::BitmapData>(flash::Stage::get(), Cs::mcw, Cs::mch, false, 0);
    flash::Matrix m;
    m.scale(1.001, 1.001);
    screenshotBmp->draw(mcGame.get(), m);
    mcScreenshot->attachBitmap(screenshotBmp, 0);
    mcScreenshot->setX(x * BW);
    mcScreenshot->setY(y * BH);
    mcScreenshot->setXScale(100.0 / 20);
    mcScreenshot->setYScale(100.0 / 20);
    mcScreenshot->setAlpha(0);
    initStars();
}

void Map::updateZoom() {
    double tmod = mt::Timer::tmod;
    if (zsens == 1) zoomCoef = Num::mm(0, (zoomCoef + 0.00005 * tmod) * 1.5, 1);
    else zoomCoef = Num::mm(0, (zoomCoef - 0.05 * tmod) * 0.8, 1);
    if (map) {
        map->setXScale(100 * (1 - zoomCoef) + zoomCoef * 2000);
        map->setYScale(100 * (1 - zoomCoef) + zoomCoef * 2000);
    }
    if (mcScreenshot) mcScreenshot->setAlpha(zoomCoef * 100);
    double tx = Cs::mcw * 0.5 - (zx + 0.5) * BW * 20;
    double ty = Cs::mch * 0.5 - (zy + 0.5) * BH * 20;
    if (map) {
        map->setX(tx * zoomCoef);
        map->setY(ty * zoomCoef);
    }
    int sens = zsens;
    double zc = zoomCoef;
    if (zoomCoef == 1) initPlay();
    else if (zoomCoef == 0) initMove();

    double cc = 1 - zc;
    double cx = Cs::mcw * 0.5 + (zx + 0.5 - 10) * BW * cc;
    double cy = Cs::mch * 0.5 + (zy + 0.5 - 10) * BH * cc;
    if (sens == 1) {
        updateStars(zc, cx, cy);
    } else {
        double dist = zc > 0 ? 50 + hx::random() * 20 / zc : INFINITY;
        if (dist < 300) {
            for (int i = 0; i < 8; i++) {
                if (zc == 0) dist = 50;
                double a = hx::random() * 6.28;
                MC mc = dm->attach("mcZoomRay", DP_INTER);
                mc->setX(cx + std::cos(a) * dist);
                mc->setY(cy + std::sin(a) * dist);
                mc->setRotation(a / 0.0174);
                mc->setXScale(100 + hx::random() * 200);
                mc->blendMode = flash::Blend::Add;
            }
        }
    }
}

void Map::initStars() {
    stars.clear();
    for (int i = 0; i < 200; i++) {
        Star s;
        s.mc = dm->attach("partStar", DP_LAYER);
        s.c = 0.2 + std::pow(hx::random(), 2) * 0.8;
        double a = i / 200.0 * 6.28;
        double dist = 20 + hx::random() * 250;
        s.dx = std::cos(a) * dist;
        s.dy = std::sin(a) * dist;
        s.fonce = 25 + std::pow(hx::random(), 2) * 250;
        stars.push_back(s);
    }
}

void Map::updateStars(double zc, double cx, double cy) {
    for (size_t i = 0; i < stars.size();) {
        Star& s = stars[i];
        double c = s.c + zc * s.fonce;
        s.mc->setX(cx + s.dx * c);
        s.mc->setY(cy + s.dy * c);
        if (c > 0.9) s.mc->setAlpha(((1 - c) / 0.1) * 100);
        else s.mc->setAlpha((1 - zc) * 100);
        if (zc < 0.0005) s.mc->setAlpha((zc / 0.0005) * 100);
        if (c > 1) {
            s.mc->removeMovieClip();
            stars.erase(stars.begin() + i);
            continue;
        }
        i++;
    }
}

// ------------------------------------------------------------------ HOLE (black holes and the retrofuser)

void Map::initHole() {
    zoomCoef = 0;
    zoomSpeed = 0;
    layer = std::make_unique<Layer>();
    layer->mc = dm->empty(DP_LAYER);
    // a copy of the map the envelope is sucked out of (the original also bends it with a displacement map)
    layer->bmp = std::make_shared<flash::BitmapData>(flash::Stage::get(), bmpBg->width, bmpBg->height, false, 0);
    layer->bmp->draw(bmpBg.get(), flash::Matrix());
    layer->mc->onPress = [] {};
    layer->mc->useHandCursor = false;
    layer->mc->attachBitmap(layer->bmp, 0);
    layer->dm = std::make_unique<mt::DepthManager>(layer->mc);
    layer->pad = layer->dm->attach("mcMiniPad", 1);
    layer->pad->setX((hx_ + 0.5) * BW);
    layer->pad->setY((hy_ + 0.5) * BH);

    initConnexion();
    warp();
    cleanAll();
}

void Map::updateHole() {
    if (!layer) {
        step = Land;
        return;
    }
    if (holeStep == 0) {
        zoomCoef = Num::mm(0, zoomCoef + ((1 - zoomCoef) * 0.1 + 0.01), 1);
        if (zoomCoef == 1) holeStep = 1;
        layer->pad->setY(Cs::mch * 0.5 + zoomCoef * 70);
        // without the hole mask, shrink the pad as it sinks
        layer->pad->setXScale(100 * (1 - zoomCoef));
        layer->pad->setYScale(100 * (1 - zoomCoef));
    } else {
        zoomSpeed += -zoomCoef * 0.25;
        zoomSpeed *= 0.9;
        zoomCoef += zoomSpeed;
        if (std::fabs(zoomSpeed) + std::fabs(zoomCoef) < 0.1) {
            layer->mc->setAlpha(layer->mc->alpha - 10);
            if (layer->mc->alpha <= 0) {
                layer->bmp->dispose();
                layer->mc->removeMovieClip();
                layer.reset();
                step = Land;
                return;
            }
        }
        layer->pad->setY(layer->pad->y - 20);
        layer->pad->setRotation(layer->pad->rotation + 20);
    }
    // stand-in for the displacement filter: the old map wobbles with the coefficient
    double s = 100 + zoomCoef * 12;
    layer->mc->setXScale(s);
    layer->mc->setYScale(100 + zoomCoef * 30);
    layer->mc->setX(Cs::mcw * 0.5 * (1 - s / 100));
    layer->mc->setY(Cs::mch * 0.5 * (1 - (100 + zoomCoef * 30) / 100));
}

void Map::updateLand() {
    if (!miniPad) {
        miniPad = dm->attach("mcMiniPad", DP_INTER);
        miniPad->setX((hx_ + 0.5) * BW);
        miniPad->setY(-(20 + Cs::mch * 0.5));
        miniPadVy = 15;
        miniPadVr = 20;
        miniPad->setRotation(0);
    }
    miniPadVy += 1;
    miniPad->setY(miniPad->y + miniPadVy);
    miniPad->setRotation(miniPad->rotation + miniPadVr);
    double gy = (hy_ + 0.5) * BH;
    if (miniPad->y > gy) {
        miniPadVy *= -0.5;
        miniPad->setY(gy);
        miniPadVr *= -0.45;
        miniPad->setRotation(0);
        if (miniPadVy > -1) {
            initMove();
            miniPad->removeMovieClip();
            miniPad = nullptr;
        }
    }
}

// Api.warp, offline: jump to the other end of the black hole; the retrofuser takes you home
void Map::warp() {
    PlayerInfo* pi = Cs::pi;
    bool done = false;
    for (auto& a : ZoneInfo::holes) {
        for (int id = 0; id < 2 && !done; id++) {
            if (a[id][0] == pi->x && a[id][1] == pi->y) {
                auto& np = a[(id + 1) % 2];
                pi->x = np[0];
                pi->y = np[1];
                done = true;
            }
        }
        if (done) break;
    }
    if (!done && pi->gotItem(MissionInfo::RETROFUSER)) {
        // the server sent the retrofuser back to the start position
        pi->x = PlayerInfo::DEFAULT_X;
        pi->y = PlayerInfo::DEFAULT_Y;
    }
    pi->ox = pi->x;
    pi->oy = pi->y;
    pi->saveCache();
    Shell::get()->requestInfos();
}

// ------------------------------------------------------------------ PLAY

void Map::initPlay() {
    cleanAll();
    for (auto& s : stars) s.mc->removeMovieClip();
    stars.clear();
    step = Play;
    game->initPlay();
    Shell::get()->levelStarted();
}

void Map::cleanAll() {
    if (bmpBg) bmpBg->dispose();
    if (bmpFog) bmpFog->dispose();
    bmpBg = bmpFog = nullptr;
    if (mcScreenshot) mcScreenshot->removeMovieClip();
    mcScreenshot = nullptr;
    if (screenshotBmp) screenshotBmp->dispose();
    screenshotBmp = nullptr;
    if (map) map->removeMovieClip();
    map = nullptr;
    bg = nullptr;
    mcMoveZone = nullptr;
    icons.clear();
    iconBlinks.clear();
    for (auto& b : menus) b.mc->removeMovieClip();
    menus.clear();
}

// ------------------------------------------------------------------ ZONES

bool Map::isZoneIn(const int pos[3]) const {
    if (pos[2] == 0) return false;
    int xMin = SX - pos[2], yMin = SY - pos[2], xMax = SX + XMAX + pos[2], yMax = SY + YMAX + pos[2];
    return pos[0] >= xMin && pos[0] < xMax && pos[1] >= yMin && pos[1] < yMax;
}

void Map::initZones() {
    zones.clear();
    for (int id = 0; id < (int)ZoneInfo::list.size(); id++)
        if (isZoneIn(ZoneInfo::list[id].pos)) zones.push_back(id);
    // zone squares, asteroid belt marks and space colours: computed exactly like the original
    // (SectorMap rasterises the cloud brushes the way Flash did, which the level colours depend on)
    SectorMap smap;
    smap.build(SX, SY);
    for (int x = 0; x < XMAX; x++)
        for (int y = 0; y < YMAX; y++) {
            zoneTable[x][y] = smap.zoneTable[x][y];
            spaceColors[x][y] = smap.spaceColors[x][y];
        }
}

// ------------------------------------------------------------------ MENU

void Map::initMenu() {
    menuTimer = 0;
    PlayerInfo* pi = Cs::pi;
    newMenu(4);  // PREFERENCES
    // EDITOR (2) and LANDER (6) need the level editor and the lander, which aren't ported yet
    if (pi->gotItem(MissionInfo::RETROFUSER)) newMenu(8, [this] { initHole(); });
}

void Map::updateMenu() {
    for (auto& b : menus) {
        double ty = Cs::mch - 37;
        b.mc->setY(b.mc->y + (ty - b.mc->y) * 0.5);
    }
    if (menu) menu->updateMenu();
}

void Map::newMenu(int id, std::function<void()> f) {
    int n = (int)menus.size();
    MC mc = dm->attach("mcMenu", DP_INTER);
    mc->setX(5 + n * 38);
    mc->setY(Cs::mch);
    mc->gotoAndStop(id + 1);
    menus.push_back({mc, id});
    flash::MovieClip* raw = mc.get();
    mc->onRollOver = [this, raw] {
        Filt::glow(raw, 2, 4, 0xFFFFFF);
        Filt::glow(raw, 10, 1, 0xFFFFFF);
        raw->blendMode = flash::Blend::Add;
        if (mcCoord) mcCoord->setAlpha(0);
    };
    mc->onRollOut = [this, raw] {
        raw->filters.clear();
        raw->blendMode = flash::Blend::Normal;
        if (mcCoord) mcCoord->setAlpha(100);
    };
    mc->onDragOver = mc->onRollOver;
    mc->onDragOut = mc->onRollOver;  // sic
    if (f) {
        mc->onPress = f;
    } else {
        mc->onPress = [this, id] {
            for (auto& b : menus)
                if (b.id == id) {
                    launchMenu(b);
                    break;
                }
        };
    }
    if (id == 0)
        if (auto s = mc->smc()) Shop::initAlien(s.get());
}

void Map::removeMenu(int id) {
    for (size_t i = 0; i < menus.size(); i++) {
        menus[i].mc->setX(5 + i * 38);
        menus[i].mc->setY(Cs::mch);
        if (menus[i].id == id) {
            menus[i].mc->removeMovieClip();
            menus.erase(menus.begin() + i);
            i--;
        }
    }
}

void Map::launchMenu(MenuButton& b) {
    if (menu) return;
    double x = b.mc->x, y = b.mc->y;
    switch (b.id) {
        case 0: menu = std::make_unique<Shop>(x, y); break;
        case 4: menu = std::make_unique<Pref>(x, y); break;
        default: break;
    }
}

void Map::switchView(bool vis) {
    for (auto& b : menus) {
        if (b.mc->onRollOut) b.mc->onRollOut();
        b.mc->visible = vis;
    }
    if (!flActive) return;
    flView = vis;
    for (auto& mc : icons) mc->visible = vis;
    if (vis) active();
    else unactive();
}

void Map::active() {
    step = Move;
    initMoveZone();
    if (bg) {
        bg->onPress = [this] { clickMap(); };
        bg->onRollOver = [] {};
        bg->onRollOut = [] {};
    }
}

void Map::unactive() {
    step = None;
    if (mcCoord) mcCoord->removeMovieClip();
    mcCoord = nullptr;
    if (mcMoveZone) mcMoveZone->removeMovieClip();
    mcMoveZone = nullptr;
    if (bg) {
        bg->onPress = nullptr;
        bg->onRollOver = nullptr;
        bg->onRollOut = nullptr;
    }
    if (mcFarWarning) mcFarWarning->removeMovieClip();
    mcFarWarning = nullptr;
}

// ------------------------------------------------------------------ MAP

void Map::initMap() {
    auto* st = flash::Stage::get();
    map = dm->empty(DP_MAP);
    mdm = std::make_unique<mt::DepthManager>(map);
    int col = Cs::COL_SPACE;
    if (Cs::pi->gotItem(MissionInfo::MODE_DIF)) col = 0x500048;
    bmpBg = std::make_shared<flash::BitmapData>(st, XMAX * BW, YMAX * BH, false, col);
    bmpFog = std::make_shared<flash::BitmapData>(st, XMAX * BW, YMAX * BH, true, 0);
    bg = mdm->empty(DP_BG);
    bg->attachBitmap(bmpBg, 0);
    bg->useHandCursor = false;
    MC mc = mdm->empty(DP_FOG);
    mc->attachBitmap(bmpFog, 0);
    mc->setAlpha(50);
    drawBg();
    drawFog();
}

void Map::drawBg() {
    const int ma = ZONE_MARGIN;
    const int TW = XMAX + ma * 2, TH = YMAX + ma * 2;
    auto seedAt = [&](int i, int j) -> mt::Random* {
        if (i < 0 || j < 0 || i >= TW || j >= TH) return nullptr;
        return seedTable[i][j].get();
    };
    struct StarP {
        double x, y, sc;
    };
    std::vector<StarP> starList;
    MC brushLight = dm->attach("mcLuz", 0);

    // CLOUDS
    for (int px = 0; px < XMAX + 2 * ma; px++)
        for (int py = 0; py < YMAX + 2 * ma; py++) {
            int x = px - ma - 5, y = py - ma;
            mt::Random* seed = seedAt(x + ma, y + ma);
            if (!seed) continue;
            double sc = 5;
            if (seed->random(70) == 0) {
                flash::Matrix m;
                double sy = (0.5 + seed->rand()) * sc;  // args reversed
                double sx = (0.5 + seed->rand()) * sc;
                m.scale(sx, sy);
                m.translate(x * BW, y * BH);
                int bi = 5, ri = 50;
                Col::RGB o;
                o.r = bi + seed->random(ri);
                o.g = bi + seed->random(ri);
                o.b = bi + seed->random(ri);
                if (auto s = brushLight->smc()) Col::setPercentColor(s.get(), 100, Col::objToCol(o));
                bmpBg->draw(brushLight.get(), m, nullptr, flash::Blend::Add);
            }
            // STARS (array literal: evaluated last to first)
            if (x >= 0 && x < XMAX && y >= 0 && y < YMAX) {
                int max = seed->random(3);
                for (int i = 0; i < max; i++) {
                    double s3 = 0.2 + seed->rand() * 0.3;
                    double s2 = (y + seed->rand()) * BH;
                    double s1 = (x + seed->rand()) * BW;
                    starList.push_back({s1, s2, s3});
                }
            }
        }

    MC brushStar = dm->attach(Cs::pi->gotItem(MissionInfo::MODE_DIF) ? "mcDifStar" : "mcStar", 0);
    for (auto& p : starList) {
        flash::Matrix m;
        m.scale(p.sc, p.sc);
        m.translate(p.x, p.y);
        bmpBg->draw(brushStar.get(), m, nullptr, flash::Blend::Add);
    }
    brushStar->removeMovieClip();
    brushLight->removeMovieClip();

    // ASTEROIDES
    MC brush = dm->attach("mcMapAsteroide", 0);
    MC bsmc = brush->smc();
    double strength = 10, noise = 0.3, freq = 0.3;
    for (int x = 0; x < XMAX; x++)
        for (int y = 0; y < YMAX; y++) {
            mt::Random* seed = seedAt(x + ma, y + ma);
            double dx = SX + x - ZoneInfo::ASTEROBELT_CX, dy = SY + y - ZoneInfo::ASTEROBELT_CY;
            double dist = std::fabs(std::sqrt(dx * dx + dy * dy) - ZoneInfo::ASTEROBELT_RAY);
            if (dist < strength) {
                double coef = dist / strength;
                if (seed->rand() > (1 - freq) + coef * freq) {
                    flash::Matrix m;
                    double px = x + (seed->rand() * 2 - 1) * noise;
                    double py = y + (seed->rand() * 2 - 1) * noise;
                    m.translate(px * BW, py * BH);
                    double rot = seed->rand() * 360;
                    double sc = 30 + 50 * (1 - coef) + seed->rand() * 40;
                    int fr = seed->random(bsmc ? bsmc->totalFrames() : 1) + 1;
                    int prc = 10 + seed->random(50);
                    if (bsmc) {
                        bsmc->setRotation(rot);
                        bsmc->setXScale(sc);
                        bsmc->setYScale(sc);
                        bsmc->gotoAndStop(fr);
                        Col::setPercentColor(bsmc.get(), prc, spaceColors[x][y]);
                    }
                    bmpBg->draw(brush.get(), m);
                }
            }
        }
    brush->removeMovieClip();

    // ZONES (the planets come from planet.swf)
    if (MC pl = Shell::mcPlanet) {
        bool vis = pl->visible;
        pl->visible = true;
        for (int id : zones) {
            const Zone& zi = ZoneInfo::list[id];
            if (auto s = pl->smc()) s->gotoAndStop(id + 1);
            double scx = zi.pos[2] * 2 * BW * 0.01, scy = zi.pos[2] * 2 * BH * 0.01;
            flash::Matrix m;
            m.scale(scx, scy);
            m.translate((zi.pos[0] - SX) * BW, (zi.pos[1] - SY) * BH);
            bmpBg->draw(pl.get(), m);
        }
        pl->visible = vis;
    }

    // LINES GRID
    auto bmp = std::make_shared<flash::BitmapData>(flash::Stage::get(), XMAX * BW, YMAX * BH, true, 0);
    unsigned c = 0x30FFFFFF;
    for (int x = 0; x < XMAX; x++) {
        flash::Rect r;
        r.add(x * BW, 0);
        r.add(x * BW + 1, YMAX * BH);
        bmp->fillRect(r, c);
    }
    for (int y = 0; y < YMAX; y++) {
        flash::Rect r;
        r.add(0, y * BH);
        r.add(XMAX * BW, y * BH + 1);
        bmp->fillRect(r, c);
    }
    bmpBg->draw(bmp.get(), flash::Matrix());
    bmp->dispose();
}

void Map::drawFog() {
    if (!bmpFog) return;
    flash::Rect all = bmpFog->rectangle();
    bmpFog->fillRect(all, 0x00000000);
    PlayerInfo* pi = Cs::pi;
    int i = 0;
    for (int x = 0; x < XMAX; x++)
        for (int y = 0; y < YMAX; y++) {
            int n = i < (int)pi->fog.size() ? pi->fog[i] : -1;
            fog[x][y] = n + 1;
            exp[x][y] = false;
            i++;
        }
    int ray = pi->radar;
    for (int x = 0; x < XMAX; x++)
        for (int y = 0; y < YMAX; y++)
            if (fog[x][y] == 2) {
                int max = ray * 2 + 1;
                for (int dx = 0; dx < max; dx++)
                    for (int dy = 0; dy < max; dy++) {
                        int nx = x + dx - ray, ny = y + dy - ray;
                        if (inGrid(nx, ny)) exp[nx][ny] = true;
                    }
            }
    const unsigned dark[2] = {0xFF000000, 0xFF330000};
    bool any = false;
    for (int x = 0; x < XMAX; x++)
        for (int y = 0; y < YMAX; y++) {
            int n = fog[x][y];
            if (n < 2) {
                flash::Rect r;
                r.add(x * BW, y * BH);
                r.add(x * BW + BW, y * BH + BH);
                bmpFog->fillRect(r, dark[n]);
                any = true;
            }
        }
    if (!any) return;  // the offline/website builds reveal the whole map: nothing else to draw
    flash::Filter fl = flash::Filter::glow(2, 8, 0xAA00FF);
    bmpFog->applyFilter(fl);
    MC skull = dm->attach("mcSkull", 0);
    const unsigned half[2] = {0x88000000, 0x88330000};
    for (int x = 0; x < XMAX; x++)
        for (int y = 0; y < YMAX; y++) {
            int n = fog[x][y];
            if (exp[x][y] && n < 2) {
                flash::Rect r;
                r.add(x * BW, y * BH);
                r.add(x * BW + BW, y * BH + BH);
                bmpFog->fillRect(r, half[n]);
            }
            if (n == 1) {
                flash::Matrix m;
                m.translate(x * BW, y * BH);
                bmpFog->draw(skull.get(), m);
            }
        }
    skull->removeMovieClip();
}

// ------------------------------------------------------------------ ACTIONS

void Map::clickMap() {
    P p = getMouseSector();
    if (!flFuel) {
        initBoxFuel();
        return;
    }
    auto rc = reach.find({p.x - SX, p.y - SY});
    if (rc != reach.end() && rc->second) callPlay(p.x, p.y);
}

void Map::callPlay(int x, int y) {
    // Api.play, offline: confirmMove(x, y, true, edited level) right after
    initConnexion();
    pendingX = x;
    pendingY = y;
    pendingMove = true;
}

void Map::confirmMove(int x, int y, bool flMinerai) {
    mcGame = dm->empty(DP_GAME);
    int gx = x - SX, gy = y - SY;
    game = std::make_unique<Game>(mcGame, inGrid(gx, gy) ? spaceColors[gx][gy] : Cs::COL_SPACE);
    startZoom(gx, gy, flMinerai);
}

void Map::initConnexion() {
    step = Connexion;
    switchView(false);
    for (auto& b : menus) b.mc->removeMovieClip();
    menus.clear();
    removeIcons();
    if (map) {
        map->onPress = nullptr;
        map->onRollOver = nullptr;
        map->onRollOut = nullptr;
        map->useHandCursor = false;
    }
}

void Map::setInfos() {
    Shell::get()->reloadPlayer();
    placeEarth();
    if (game) {
        game.reset();
        mcGame = nullptr;
        Sprite::collectGarbage();
    }
    init();
}

// Map.setInfos: Earth is placed from the player id, 1500-2000 sectors away
void Map::placeEarth() {
    Zone* earth = nullptr;
    for (auto& z : ZoneInfo::list)
        if (z.name == "Terre") earth = &z;
    if (!earth) return;
    mt::Rand seed(Cs::pi->pid);
    if (Cs::pi->gotItem(MissionInfo::MODE_DIF)) seed.random(100);
    int ray = 1500 + seed.random(500);
    int a = seed.random(ray);
    earth->pos[0] = a * (seed.random(2) * 2 - 1);
    int b = ray - std::abs(earth->pos[0]);
    earth->pos[1] = hx::Int((double)b * (seed.random(2) * 2 - 1));
}

// ------------------------------------------------------------------ HERO / ICONS

void Map::updateHero() {
    if (mcMoveZone) mcMoveZone->visible = false;
    heroMove->c = std::min(heroMove->c + 0.04 * mt::Timer::tmod, 1.0);
    double x = heroMove->sx * (1 - heroMove->c) + heroMove->ex * heroMove->c;
    double y = heroMove->sy * (1 - heroMove->c) + heroMove->ey * heroMove->c;
    heroMove->icon->setX(x * BW);
    heroMove->icon->setY(y * BH);
    if (heroMove->c == 1) {
        if (mcMoveZone) mcMoveZone->visible = true;
        heroMove.reset();
    }
}

void Map::initIcons() {
    icons.clear();
    iconBlinks.clear();
    iconBlink = 0;
    PlayerInfo* pi = Cs::pi;

    // BLACK HOLES
    for (auto& a : ZoneInfo::holes)
        for (auto& p : a) {
            int rx = p[0] - SX, ry = p[1] - SY;
            if (inGrid(rx, ry) && rx == hx_ && ry == hy_) newMenu(1, [this] { initHole(); });
        }

    // SHOPS
    for (int x = 0; x < XMAX; x++)
        for (int y = 0; y < YMAX; y++) {
            int wx = SX + x, wy = SY + y;
            if (pi->gotItem(MissionInfo::MAP_SHOP) || (wx == -2 && wy == 3)) {
                double dst = std::sqrt((double)wx * wx + (double)wy * wy);
                mt::Random* seed = seedTable[x + ZONE_MARGIN][y + ZONE_MARGIN].get();
                if (seed->random(hx::Int(40 + std::pow(dst, 1.4))) == 0) {
                    displayIcon(1, x, y, false);
                    if (x == hx_ && y == hy_) newMenu(0);
                }
            }
        }

    // ITEMS
    for (int id = 0; id < (int)MissionInfo::ITEMS.size(); id++) {
        const ItemPos& o = MissionInfo::ITEMS[id];
        if (!o.has) continue;
        if (o.x > SX && o.x < SX + XMAX && o.y > SY && o.y < SY + YMAX) {
            if (pi->items.get(id) == 1 ||
                (o.fam == 1 && pi->shopItems.get(ShopInfo::MISSILE_MAP) == 1 && !pi->gotItem(id)))
                displayIcon(id + 10, o.x - SX, o.y - SY, true);
        }
    }

    // HERO
    MC h = displayIcon(0, hx_, hy_);
    int dx = pi->ox - pi->x, dy = pi->oy - pi->y;
    int sum = std::abs(dx) + std::abs(dy);
    if (sum > 0 && sum < 10) {
        heroMove = std::make_unique<HeroMove>(HeroMove{h, 0.0, hx_ + dx, hy_ + dy, hx_, hy_});
        updateHero();
    }

    // START CLICK
    if (pi->missions.get(0) == 0) {
        MC mc = mdm->attach("mcStartClick", DP_ICONS);
        mc->setX(10.5 * BW);
        mc->setY(10.5 * BH);
        if (auto f = mc->field("field")) f->setText(Text::str("START_CLIC_GREEN"));
        icons.push_back(mc);
        iconBlinks.push_back(false);
    }

    // MINE ZONE
    if (pi->square.size() >= 3) {
        MC mc = mdm->attach("mcMineZone", DP_ICONS);
        mc->setX((pi->square[0] - SX) * BW);
        mc->setY((pi->square[1] - SY) * BH);
        if (auto s = mc->smc()) {
            s->setXScale(pi->square[2] * BW);
            s->setYScale(pi->square[2] * BH);
        }
        icons.push_back(mc);
        iconBlinks.push_back(true);
        int frame = pi->gotItem(MissionInfo::EVASION) ? 2 : 1;
        mc->gotoAndStop(frame);
        if (auto s = mc->smc()) s->gotoAndStop(frame);
        Filt::glow(mc.get(), 10, 1, frame == 1 ? 0x00FF00 : 0xFF0000);
    }
}

MC Map::displayIcon(int id, int x, int y, bool flBlink) {
    MC mc = mdm->attach("mcMapIcon", DP_ICONS);
    mc->setX(x * BW);
    mc->setY(y * BH);
    if (id >= 10) {
        mc->gotoAndStop(11);
        if (auto s = mc->smc()) s->gotoAndStop((id - 10) + 1);
    } else {
        mc->gotoAndStop(id + 1);
    }
    icons.push_back(mc);
    iconBlinks.push_back(flBlink);
    return mc;
}

void Map::updateIcons() {
    if (!flView) return;
    iconBlink += mt::Timer::tmod;
    if (iconBlink > 25) {
        iconBlink = 0;
        for (size_t i = 0; i < icons.size(); i++) {
            if (!iconBlinks[i]) continue;
            MC& mc = icons[i];
            if (mc->alpha == 0) {
                mc->setAlpha(100);
            } else {
                mc->setAlpha(0);
                iconBlink = 20;
            }
        }
    }
}

void Map::removeIcons() {
    for (auto& mc : icons) mc->removeMovieClip();
    icons.clear();
    iconBlinks.clear();
}

// ------------------------------------------------------------------ MOVES

void Map::initMoveZone() {
    if (mcMoveZone) mcMoveZone->removeMovieClip();
    mcMoveZone = mdm->empty(DP_MOVE);
    mcMoveZone->useHandCursor = true;
    mcMoveZone->setX(hx_ * BW);
    mcMoveZone->setY(hx_ * BH);  // sic (hx == hy)
    mcMoveZone->blendMode = flash::Blend::Add;
    moveZoneDec = 0;
    Filt::glow(mcMoveZone.get(), 2, 100, 0xFFFFFF);
    mcMoveZone->setAlpha(0);
    mcMoveZone->onPress = [this] { clickMap(); };

    reach.clear();
    reach[{hx_, hy_}] = true;
    std::vector<std::array<int, 2>> zone{{hx_, hy_}};
    std::vector<std::array<int, 2>> list{{hx_, hy_}};
    for (int i = 0; i < Cs::pi->engine; i++) {
        zone = extendZone(zone);
        for (auto& p : zone) list.push_back(p);
    }
    if (Cs::pi->missions.get(0) != 0) {
        list.erase(list.begin());
        reach[{hx_, hy_}] = false;
    }
    int fr = flFuel ? 1 : 2;
    mt::DepthManager mdmz(mcMoveZone);
    for (auto& p : list) {
        MC mc = mdmz.attach("mcMove", 0);
        mc->setX((p[0] - hx_) * BW);
        mc->setY((p[1] - hy_) * BH);
        mc->gotoAndStop(fr);
    }
}

std::vector<std::array<int, 2>> Map::extendZone(const std::vector<std::array<int, 2>>& zone) {
    std::vector<std::array<int, 2>> list;
    for (auto& p : zone) {
        bool open = inGrid(p[0], p[1]) && (fog[p[0]][p[1]] == 2 || exp[p[0]][p[1]]);
        if (!open) continue;
        for (auto& d : Cs::DIR) {
            int nx = p[0] + d[0], ny = p[1] + d[1];
            bool storable = nx >= 0 && nx < XMAX;  // reach[nx] only exists inside the grid's columns
            if (!storable || reach.find({nx, ny}) == reach.end()) {
                if (storable) reach[{nx, ny}] = true;
                list.push_back({nx, ny});
            }
        }
    }
    return list;
}

Map::P Map::getMouseSector() const {
    flash::Point m = map ? map->mouse() : flash::Point{0, 0};
    int gx = hx::Int(m.x / BW), gy = hx::Int(m.y / BH);
    int x = hx::Int(Num::mm(0, gx, XMAX - 1) + Cs::pi->x - hx::Int(XMAX * 0.5));
    int y = hx::Int(Num::mm(0, gy, YMAX - 1) + Cs::pi->y - hx::Int(YMAX * 0.5));
    return {x, y};
}

// ------------------------------------------------------------------ BOX

void Map::initBoxFuel() {
    if (box) return;
    box = dm->attach("boxFuel", DP_INTER);
    if (auto s = box->smc()) s->onPress = [] {};
    if (auto f = box->field("fieldTitle")) f->setText(Text::str("FUEL_TITLE"));
    if (auto f = box->field("fieldText")) f->setHtmlText(Text::str("FUEL_TEXT"));
    if (auto f = box->field("fieldBut")) f->setText(Text::str("FUEL_BANK"));
    if (MC but = box->clip("but")) {
        flash::MovieClip* b = but.get();
        b->stop();
        // the interstellar bank was online; offline the button just closes the box
        b->onPress = [this, b] {
            b->gotoAndStop(3);
            if (box) box->removeMovieClip();
            box = nullptr;
        };
        b->onRollOver = [b] { b->gotoAndStop(2); };
        b->onRollOut = [b] { b->gotoAndStop(1); };
        b->onDragOver = b->onRollOver;
        b->onDragOut = b->onRollOut;
        b->onRelease = b->onRollOver;
        b->onReleaseOutside = b->onRollOut;
    }
}

// ================================================================== Menu

Menu::Menu(double x, double y) : map(Map::me), icx(x), icy(y) {
    root = map->dm->empty(Map::DP_INTER);
    dm = std::make_unique<mt::DepthManager>(root);
    initSquare(0);
    if (map->game) map->game->switchView(false);
    map->switchView(false);
}

Menu::~Menu() {
    if (root) root->removeMovieClip();
}

void Menu::init() {
    step = 1;
    if (mcDraw) mcDraw->removeMovieClip();
    mcDraw = nullptr;
}

void Menu::updateMenu() {
    if (dead) return;
    switch (step) {
        case 0: updateSquare(); break;
        case 1: update(); break;
    }
}

void Menu::initSquare(double c) {
    step = 0;
    sens = -(int)(c * 2 - 1);
    coef = c;
    mcDraw = dm->empty(0);
}

void Menu::updateSquare() {
    coef = Num::mm(0, coef + 0.07 * sens * mt::Timer::tmod, 1);
    mcDraw->clear();
    mcDraw->lineStyle(1, 0xFFFFFF, 100);
    double sx = icx + 16, sy = icy + 16, sr = 16;
    double ex = Cs::mcw * 0.5, ey = Cs::mch * 0.5, er = Cs::mcw * 0.5 - MARGIN;
    double x = sx * (1 - coef) + ex * coef;
    double y = sy * (1 - coef) + ey * coef;
    double c = std::pow(coef, 3);
    double r = sr * (1 - c) + er * c;
    double rx = r, ry = r * Cs::mch / Cs::mcw;
    mcDraw->moveTo(x - rx, y - ry);
    mcDraw->lineTo(x + rx, y - ry);
    mcDraw->lineTo(x + rx, y + ry);
    mcDraw->lineTo(x - rx, y + ry);
    mcDraw->lineTo(x - rx, y - ry);
    if (coef == 1) init();
    else if (coef == 0) kill();
}

void Menu::kill() {
    dead = true;
    root->removeMovieClip();
    root = nullptr;
    Map* m = map;
    m->deadMenu = std::move(m->menu);  // freed on the next map update, not while running
    m->switchView(true);
    if (m->game) m->game->switchView(true);
}

void Menu::quit() {
    dm->destroy();
    initSquare(1);
}

// ================================================================== Shop

const std::vector<std::vector<int>> Shop::PALETTE = {
    {0x009933, 0x869405, 0x8D730C, 0x246F11},
    {0x4176F1, 0x0F48CE, 0x0C389E, 0x12CBA6, 0x0FA485, 0x0C856C, 0xE8390F, 0xA8290B, 0x681A06, 0xF2DC15,
     0xDCB10A, 0xDCB10A, 0x7D5924, 0x5C421B, 0x8D61D1, 0x5A2E9E, 0x402170, 0x838327, 0x686820, 0x55551A},
    {0xF7FADA, 0xEFF5B4, 0xFDE4CE, 0xFCD5AD, 0xECDFCE, 0xDDC8AA, 0xDDE0B8, 0xDDE0B8, 0xD1D79F},
    {0xFF0000, 0xFF8800, 0x0044DD, 0x33DD00},
};

void Shop::init() {
    Menu::init();
    skin = dm->attach("mcShop", 1);
    skin->setX(MARGIN);
    skin->setY(MARGIN * ((double)Cs::mch / Cs::mcw));
    sdm = std::make_unique<mt::DepthManager>(skin->clip("screen"));
    if (auto g = skin->clip("gem")) g->visible = false;
    if (auto q = skin->clip("quit")) q->gotoAndStop(1);
    if (auto b = skin->clip("buy")) b->gotoAndStop(2);
    Filt::glow(skin.get(), 2, 4, 0xFFFFFF);
    active(skin->clip("quit"), [this] { quit(); });
    unactive(skin->clip("buy"));
    initSeed();
    if (auto scr = skin->clip("screen"))
        if (auto s = scr->smc()) {
            s->onPress = [this] { unselect(); };
            s->useHandCursor = false;
        }
}

void Shop::initSeed() {
    int n = Cs::pi->x * (967 + Cs::pi->y) + Cs::pi->y;
    seed = std::make_unique<mt::OldRandom>(n);
    if (auto v = skin->clip("vig")) initAlien(v.get());
    genContent();
}

void Shop::genContent() {
    PlayerInfo* pi = Cs::pi;
    std::vector<int> list;
    double dst = std::sqrt((double)pi->x * pi->x + (double)pi->y * pi->y);
    for (int id = 0; id < (int)ShopInfo::ITEMS.size(); id++) {
        const ShopItem& info = ShopInfo::ITEMS[id];
        // `pb[1] <= dst` compiles to !(pb[1] > dst): a missing distance always passes
        bool near = info.pb.size() < 2 || !(info.pb[1] > dst);
        if (near && !info.pb.empty() && seed->random(info.pb[0]) == 0)
            if (isVisible(id)) list.push_back(id);
    }
    slots.clear();
    double ma = 4;
    for (int i = 0; i < 12; i++) {
        MC mc = sdm->attach("mcShopSlot", 0);
        mc->setX(ma + (i % 2) * 117);
        mc->setY(ma + (i / 2) * (52 + ma - 1));
        if (i < (int)list.size()) {
            int n = list[i];
            mc->stop();
            if (auto ic = mc->clip("icon")) ic->gotoAndStop(n + 1);
            if (auto f = mc->field("field")) f->setText(Text::at("SHOP_ITEM_NAMES", n));
            int idx = (int)slots.size();
            slots.push_back({mc, n});
            if (isBuyable(n)) {
                mc->onPress = [this, idx] { select(idx); };
            } else {
                mc->setAlpha(50);
                mc->blendMode = flash::Blend::Overlay;
            }
        } else {
            mc->gotoAndStop(2);
        }
    }
}

void Shop::cleanContent() {
    for (auto& s : slots) s.mc->removeMovieClip();
    slots.clear();
}

void Shop::initAlien(flash::MovieClip* mc, const mt::OldRandom* seed) {
    mt::OldRandom local(0);
    if (seed) {
        local = *seed;
    } else {
        int n = Cs::pi->x * (967 + Cs::pi->y) + Cs::pi->y;
        local = mt::OldRandom(n);
    }
    mt::Bouille b;
    b.colorDecal = 10;
    b.firstDecal = 0;
    b.palette = PALETTE;
    b.paletteRedirect = {0, 1, 1, 2, 3};
    for (int i = 0; i < 20; i++) b.skin.push_back(local.random(200));
    b.framize(mc);
    b.colorize(mc);
}

bool Shop::isVisible(int n) const {
    PlayerInfo* pi = Cs::pi;
    bool flHave = pi->shopItems.get(n) == 1;
    switch (n) {
        case ShopInfo::RADAR: return !pi->gotItem(MissionInfo::RADAR_OK);
        case ShopInfo::AMMO: return true;
        case ShopInfo::LIFE: return !flHave && !pi->gotItem(MissionInfo::MODE_DIF);
        case ShopInfo::DRONE: return pi->drone < 10;
        case ShopInfo::PODS:
            return !flHave && pi->gotItem(MissionInfo::LANDER_REACTOR) && pi->x == -12 && pi->y == -1 && pi->getLife() > 0;
        case ShopInfo::PODS_EXTEND_0:
        case ShopInfo::PODS_EXTEND_1:
        case ShopInfo::PODS_EXTEND_2: return !flHave && pi->shopItems.get(ShopInfo::PODS) == 1;
        case ShopInfo::LANDER_REACTOR_0:
        case ShopInfo::LANDER_REACTOR_1:
        case ShopInfo::LANDER_REACTOR_2: return !flHave && pi->gotItem(MissionInfo::LANDER_REACTOR);
        case ShopInfo::MINE_0:
        case ShopInfo::MINE_1:
        case ShopInfo::MINE_2: return !flHave && pi->gotItem(MissionInfo::MINES);
    }
    if (n >= ShopInfo::MISSILE && n < ShopInfo::MISSILE + 3) return pi->missileMax > 0 && !flHave;
    return !flHave;
}

bool Shop::isBuyable(int n) const {
    PlayerInfo* pi = Cs::pi;
    switch (n) {
        case ShopInfo::AMMO: return pi->missileMax > pi->missile;
        case ShopInfo::DRONE_PERFO:
        case ShopInfo::DRONE_SPEED:
        case ShopInfo::DRONE_CONVERTER:
        case ShopInfo::DRONE_COLLECTOR: return pi->drone > 0;
    }
    return true;
}

void Shop::select(int i) {
    if (selection >= 0) unselect();
    selection = i;
    slots[i].mc->blendMode = flash::Blend::Add;
    int price = Cs::pi->getPrice(slots[i].id);
    if (auto f = skin->field("field")) f->setText(std::to_string(price));
    MC gem = skin->clip("gem");
    if (gem) gem->visible = true;
    auto field = skin->field("field");
    if (Cs::pi->minerai >= price) {
        active(skin->clip("buy"), [this] { buyItem(); });
        if (gem) Col::setPercentColor(gem.get(), 0, 0);
        if (field) Col::setPercentColor(field.get(), 0, 0);
    } else {
        if (gem) Col::setPercentColor(gem.get(), 100, 0xFF0000);
        if (field) Col::setPercentColor(field.get(), 100, 0xFF0000);
    }
}

void Shop::unselect() {
    if (selection >= 0 && selection < (int)slots.size()) slots[selection].mc->blendMode = flash::Blend::Normal;
    selection = -1;
    if (auto f = skin->field("field")) f->setText("");
    if (auto g = skin->clip("gem")) g->visible = false;
    unactive(skin->clip("buy"));
}

void Shop::buyItem() {
    if (selection < 0) return;
    int id = slots[selection].id;
    // the original also did Cs.pi.radar++ here before Api.buyItem added it again; the website's server
    // only counted it once, which is what the port keeps
    if (auto b = skin->clip("buy")) b->blendMode = flash::Blend::Normal;
    // Api.buyItem, offline
    Cs::pi->buyShopItem(id);
    Cs::pi->saveCache();
    if (id == ShopInfo::RADAR) map->drawFog();
    unselect();
    cleanContent();
    initSeed();
}

void Shop::quit() {
    skin->removeMovieClip();
    initSquare(1);
}

void Shop::active(const MC& mc, std::function<void()> f) {
    if (!mc) return;

    flash::MovieClip* raw = mc.get();
    mc->onRollOver = [raw] { raw->blendMode = flash::Blend::Add; };
    mc->onRollOut = [raw] { raw->blendMode = flash::Blend::Normal; };
    mc->onDragOut = mc->onRollOut;
    mc->onRelease = std::move(f);
    mc->useHandCursor = true;
    if (auto s = mc->smc()) s->setAlpha(100);
}

void Shop::unactive(const MC& mc) {
    if (!mc) return;
    mc->onRollOver = nullptr;
    mc->onRollOut = nullptr;
    mc->onDragOut = nullptr;
    mc->onRelease = nullptr;
    mc->useHandCursor = false;
    if (auto s = mc->smc()) s->setAlpha(20);
}

// ================================================================== Pref

void Pref::init() {
    Menu::init();
    skin = dm->attach("mcPref", 0);
    if (auto s = skin->smc()) {
        s->onPress = [] {};
        s->useHandCursor = false;
    }
    if (auto f = skin->field("fieldTitle")) f->setText(Text::str("PREF_TITLE"));
    if (auto f = skin->field("fieldMouse")) f->setText(Text::str("PREF_MOUSE"));
    if (auto f = skin->field("fieldQuality")) f->setText(Text::str("PREF_QUALITY"));

    // BUTTONS
    double x = Cs::mcw;
    MC q = dm->attach("mcWorldBut", 1);
    q->gotoAndStop(1);
    x -= q->width() + 4;
    q->setX(x);
    q->setY(Cs::mch - 22);
    flash::MovieClip* qr = q.get();
    q->onRollOver = [qr] { qr->blendMode = flash::Blend::Add; };
    q->onRollOut = [qr] { qr->blendMode = flash::Blend::Normal; };
    q->onDragOut = q->onRollOut;
    q->onPress = [this] { quit(); };

    // SLIDERS (startDrag between START and START+RANGE)
    for (int i = 0; i < 2; i++) {
        MC mc = skin->clip(i == 0 ? "but0" : "but1");
        if (!mc) continue;
        mc->onPress = [this, i] { dragging = i; };
    }

    // BOOLS
    bools.clear();
    for (int id = 0; id < 4; id++) {
        MC mc = dm->attach("mcPrefBool", 1);
        mc->setY(244 + id * 24);
        if (auto f = mc->field("field")) f->setText(Text::at("PREF_FLAGS", id));
        if (auto b = mc->clip("box")) {
            b->gotoAndStop(Cs::PREF_BOOLS[id] ? 2 : 1);
            b->onPress = [this, id] { toggleBool(id); };
        }
        bools.push_back(mc);
    }

    // DEFAULT
    auto& so = flash::Stage::get()->sharedObject("pref");
    if (auto b = skin->clip("but0")) b->setX(START + RANGE * so.value("mouse", 0.2));
    if (auto b = skin->clip("but1")) b->setX(START + RANGE * so.value("gfx", 1.0));
}

void Pref::update() {
    if (dragging < 0) return;
    MC mc = skin ? skin->clip(dragging == 0 ? "but0" : "but1") : nullptr;
    if (!mc) {
        dragging = -1;
        return;
    }
    flash::Point m = skin->mouse();
    mc->setX(Num::mm(START, m.x, START + RANGE));
    if (!flash::Stage::get()->mouseDown) {
        setVal(dragging, (mc->x - START) / RANGE);
        dragging = -1;
    }
}

void Pref::toggleBool(int id) {
    auto* st = flash::Stage::get();
    auto& so = st->sharedObject("pref");
    if (!so["bools"].is_array()) so["bools"] = nlohmann::json::array();
    while ((int)so["bools"].size() <= id) so["bools"].push_back(false);
    so["bools"][id] = !(so["bools"][id].is_boolean() && so["bools"][id].get<bool>());
    st->flushSharedObject("pref");
    Cs::loadPref();
    if (auto b = bools[id]->clip("box")) b->gotoAndStop(Cs::PREF_BOOLS[id] ? 2 : 1);
}

void Pref::setVal(int id, double c) {
    auto* st = flash::Stage::get();
    auto& so = st->sharedObject("pref");
    so[id == 0 ? "mouse" : "gfx"] = c;
    st->flushSharedObject("pref");
    Cs::loadPref();
}

}  // namespace navi
