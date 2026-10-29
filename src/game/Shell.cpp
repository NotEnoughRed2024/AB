#include "Shell.h"

#include <cmath>

#include "Elements.h"
#include "Map.h"

namespace Col = mt::Col;

Shell* Shell::instance = nullptr;
std::unique_ptr<mt::DepthManager> Shell::dm;
MC Shell::mcPlanet;

static constexpr int DP_MAP = 1;

void Shell::hideMouse() {
    flash::Stage::get()->mouseHidden = true;
    flash::Stage::get()->applyCursor();
}
void Shell::showMouse() {
    flash::Stage::get()->mouseHidden = false;
    flash::Stage::get()->applyCursor();
}

// ------------------------------------------------------------------ SectorMap (navi.Map.drawBg)

void SectorMap::build(int sx, int sy) {
    SX = sx;
    SY = sy;
    const int ma = ZONE_MARGIN;
    const int TW = XMAX + ma * 2, TH = YMAX + ma * 2;
    std::vector<std::vector<std::unique_ptr<mt::Random>>> seedTable(TW);
    for (int x = 0; x < TW; x++) {
        seedTable[x].resize(TH);
        for (int y = 0; y < TH; y++) {
            int px = x + SX - ma, py = y + SY - ma;
            int n = hx::Int((double)px * (1000 + py) + py);
            seedTable[x][y] = std::make_unique<mt::Random>(n);
        }
    }
    auto seedAt = [&](int i, int j) -> mt::Random* {
        if (i < 0 || j < 0 || i >= TW || j >= TH) return nullptr;
        return seedTable[i][j].get();
    };

    int col = Cs::COL_SPACE;
    if (Cs::pi && Cs::pi->gotItem(MissionInfo::MODE_DIF)) col = 0x500048;
    // only the pixels sampled for spaceColors matter for gameplay: rasterise just those
    auto bmpBg = std::make_shared<flash::BitmapData>(flash::Stage::get(), XMAX * BW, YMAX * BH, false, col, true);
    bmpBg->softMask.assign((size_t)bmpBg->width * bmpBg->height, 0);
    for (int x = 0; x < XMAX; x++)
        for (int y = 0; y < YMAX; y++) bmpBg->softMask[(size_t)(y * BH) * bmpBg->width + x * BW] = 1;

    MC brushLight = Shell::dm->attach("mcLuz", 0);
    // CLOUDS (+ star rng consumption)
    for (int px = 0; px < XMAX + 2 * ma; px++)
        for (int py = 0; py < YMAX + 2 * ma; py++) {
            int x = px - ma, y = py - ma;
            x -= 5;
            mt::Random* seed = seedAt(x + ma, y + ma);
            if (!seed) continue;  // undefined seed: every call is a no-op in Flash
            double sc = 5;
            if (seed->random(70) == 0) {
                flash::Matrix m;
                double sy = (0.5 + seed->rand()) * sc;  // args reversed
                double sxx = (0.5 + seed->rand()) * sc;
                m.scale(sxx, sy);
                m.translate(x * BW, y * BH);
                int bi = 5, ri = 50;
                Col::RGB o;
                o.r = bi + seed->random(ri);
                o.g = bi + seed->random(ri);
                o.b = bi + seed->random(ri);
                if (auto s = brushLight->smc()) Col::setPercentColor(s.get(), 100, Col::objToCol(o));
                bmpBg->draw(brushLight.get(), m, nullptr, flash::Blend::Add);
            }
            if (x >= 0 && x < XMAX && y >= 0 && y < YMAX) {
                int max = seed->random(3);
                for (int i = 0; i < max; i++) {
                    seed->rand();
                    seed->rand();
                    seed->rand();
                }
            }
        }
    brushLight->removeMovieClip();
    for (int x = 0; x < XMAX; x++)
        for (int y = 0; y < YMAX; y++) spaceColors[x][y] = (int)bmpBg->getPixel(x * BW, y * BH);

    // ZONES (initZones runs before drawBg; asteroid marks then override)
    for (int x = 0; x < XMAX; x++)
        for (int y = 0; y < YMAX; y++) zoneTable[x][y] = -1;
    for (int id = 0; id < (int)ZoneInfo::list.size(); id++) {
        const Zone& z = ZoneInfo::list[id];
        int r = z.pos[2];
        for (int wx = z.pos[0] - r; wx <= z.pos[0] + r - 1; wx++)
            for (int wy = z.pos[1] - r; wy <= z.pos[1] + r - 1; wy++)
                if (ZoneInfo::isInCircle(wx, wy, z.pos[0], z.pos[1], r)) {
                    int x = wx - SX, y = wy - SY;
                    if (x >= 0 && x < XMAX && y >= 0 && y < YMAX) zoneTable[x][y] = id;
                }
    }
    // ASTEROIDS
    MC brush = Shell::dm->attach("mcMapAsteroide", 0);
    int frames = brush->smc() ? brush->smc()->totalFrames() : 1;
    brush->removeMovieClip();
    double strength = 10, freq = 0.3;
    for (int x = 0; x < XMAX; x++)
        for (int y = 0; y < YMAX; y++) {
            mt::Random* seed = seedAt(x + ma, y + ma);
            double dx = SX + x - ZoneInfo::ASTEROBELT_CX, dy = SY + y - ZoneInfo::ASTEROBELT_CY;
            double dist = std::fabs(std::sqrt(dx * dx + dy * dy) - ZoneInfo::ASTEROBELT_RAY);
            if (dist < strength) {
                double coef = dist / strength;
                if (seed->rand() > (1 - freq) + coef * freq) {
                    seed->rand();
                    seed->rand();
                    seed->rand();
                    seed->rand();
                    seed->random(frames);
                    seed->random(50);
                    zoneTable[x][y] = ZoneInfo::ASTEROBELT;
                }
            }
        }
}

// ------------------------------------------------------------------ Shell

Shell::Shell() { instance = this; }

Shell::~Shell() {
    map.reset();
    Sprite::collectGarbage();
    dm.reset();
    mcPlanet.reset();
    instance = nullptr;
}

bool Shell::init(const std::string& assetsDir, const std::string& dataDir, const std::string& lang) {
    auto* st = flash::Stage::get();
    if (!loadGameData(dataDir)) return false;
    Text::load(dataDir, lang);
    dm = std::make_unique<mt::DepthManager>(st->root);
    dm->attach("mcManagerBg", 0);
    Cs::init();
    // PLANET (MovieClipLoader.loadClip("planet.swf"))
    if (flash::Library* pl = st->loadLibrary(assetsDir, "planet")) {
        mcPlanet = dm->empty(0);
        mcPlanet->visible = false;
        mcPlanet->loadRoot(pl);
    }
    // Manager.initLeReste: the map, waiting for Api.askInfos -> setInfos
    map = std::make_unique<navi::Map>(dm->empty(DP_MAP));
    map->setInfos();
    return true;
}

void Shell::reloadPlayer() {
    pi = std::make_unique<PlayerInfo>();
    Cs::pi = pi.get();
    pi->loadCache();
}

Game* Shell::game() const { return map ? map->game.get() : nullptr; }

void Shell::levelStarted() { levelStartMs = SDL_GetTicks(); }

void Shell::update() {
    if (pendingInfos) {
        pendingInfos = false;
        map->setInfos();
    }
    if (directPending && map->isMoving()) {
        directPending = false;
        map->callPlay(directX, directY);
    }
    Game* g = game();
    if (g && map->isPlaying()) {
        // test hooks: AB_TEST_OPTION=n applies capsule n after 60 frames, AB_TEST_PAUSE pauses at frame 120
        static int testFrame = 0;
        testFrame++;
        if (const char* o = SDL_getenv("AB_TEST_OPTION"))
            if (testFrame == 60 && g->pad) g->getOption(std::atoi(o));
        if (SDL_getenv("AB_TEST_PAUSE") && (testFrame == 120 || testFrame == 200)) g->togglePause();
        if (Game::PLAY_AUTO) {
            // test harness: hold the button until the first ball is launched, then play like the demo
            if (g->flFirstBall && !g->flPress) g->mouseDown();
            else if (!g->flFirstBall && g->flPress && g->pad && g->pad->type == Cs::PAD_STANDARD) g->mouseUp();
            g->updateAuto();
        }
    }
    map->update();
}

void Shell::initConnexion() {
    if (map) map->initConnexion();
}

void Shell::playDirect(int wx, int wy) {
    // testing: put the player on that sector and click it as soon as the map is ready
    Cs::pi->x = Cs::pi->ox = wx;
    Cs::pi->y = Cs::pi->oy = wy;
    Cs::pi->saveCache();
    map->setInfos();
    directX = wx;
    directY = wy;
    directPending = true;
}

// Api.endGame, offline branch ("emulation de la partie serveur")
void Shell::endGame(int x, int y, bool victory, int min, int missiles, int itemId, const std::vector<int>& shopItems) {
    PlayerInfo* p = Cs::pi;
    p->stats.levels++;
    if (victory) p->stats.cleared++;
    else p->stats.lost++;
    p->stats.mineralsTotal += std::max(0, min);
    p->stats.bestMinerals = std::max(p->stats.bestMinerals, min);
    if (missiles < p->missile) p->stats.missilesFired += p->missile - missiles;
    if (levelStartMs > 0) p->stats.playSeconds += (long long)((SDL_GetTicks() - levelStartMs) / 1000);
    levelStartMs = 0;
    p->minerai += min;
    p->missile = missiles;
    p->addItem(itemId);
    // the server remembered every sector played: cleared ones lose their minerals and light up the map,
    // lost ones get a skull
    if (victory) p->sectors[{x, y}] = 1;
    else if (p->sectorState(x, y) != 1) p->sectors[{x, y}] = 0;
    // the website remembered where the envelope came from, so the map can animate the move
    p->ox = p->x;
    p->oy = p->y;
    if (victory) {
        p->x = x;
        p->y = y;
    }
    p->checkMission();
    for (int id : shopItems) p->shopItems.set(id, 0);
    p->saveCache();
    if (Game* g = game()) g->kill();
    pendingInfos = true;  // haxe.Timer.delay(Api.setInfos): never tear the game down mid-update
}
