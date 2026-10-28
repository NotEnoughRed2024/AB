#include "Shell.h"

#include <cmath>
#include <sstream>

#include "Elements.h"

namespace Col = mt::Col;

Shell* Shell::instance = nullptr;
std::unique_ptr<mt::DepthManager> Shell::dm;
MC Shell::mcPlanet;

static constexpr int DP_MAP = 1;

void Shell::hideMouse() { SDL_HideCursor(); }
void Shell::showMouse() { SDL_ShowCursor(); }

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
    game.reset();
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
    setInfos();
    return true;
}

void Shell::setInfos() {
    // Map.setInfos: fresh PlayerInfo from the save, then back to the map
    pi = std::make_unique<PlayerInfo>();
    Cs::pi = pi.get();
    pi->loadCache();
    if (game) {
        game.reset();
        mcGame = nullptr;
        Sprite::collectGarbage();
    }
    openSelect();
}

void Shell::update() {
    if (pendingInfos) {
        pendingInfos = false;
        setInfos();
        return;
    }
    if (game) {
        // test hooks: AB_TEST_OPTION=n applies capsule n after 60 frames, AB_TEST_PAUSE pauses at frame 120
        static int testFrame = 0;
        testFrame++;
        if (const char* o = SDL_getenv("AB_TEST_OPTION"))
            if (testFrame == 60 && game->pad) game->getOption(std::atoi(o));
        if (SDL_getenv("AB_TEST_PAUSE") && (testFrame == 120 || testFrame == 200)) game->togglePause();
        if (Game::PLAY_AUTO) {
            // test harness: hold the button until the first ball is launched, then play like the demo
            if (game->flFirstBall && !game->flPress) game->mouseDown();
            else if (!game->flFirstBall && game->flPress && game->pad && game->pad->type == Cs::PAD_STANDARD) game->mouseUp();
            game->updateAuto();
        }
        game->update();
    }
}

void Shell::initConnexion() {}

void Shell::playDirect(int wx, int wy) {
    closeSelect();
    Cs::pi->x = wx;
    Cs::pi->y = wy;
    openSelect();
    startLevel(wx, wy);
}

// Api.endGame, offline branch ("emulation de la partie serveur")
void Shell::endGame(int x, int y, bool victory, int min, int missiles, int itemId, const std::vector<int>& shopItems) {
    PlayerInfo* p = Cs::pi;
    p->minerai += min;
    p->missile = missiles;
    p->addItem(itemId);
    if (victory) {
        p->x = x;
        p->y = y;
    }
    p->checkMission();
    for (int id : shopItems) p->shopItems.set(id, 0);
    p->saveCache();
    if (game) game->kill();
    pendingInfos = true;  // haxe.Timer.delay(Api.setInfos): never tear the game down mid-update
}

// ------------------------------------------------------------------ sector select (stand-in for navi.Map)

void Shell::openSelect() {
    showMouse();
    smap.build(Cs::pi->x - SectorMap::XMAX / 2, Cs::pi->y - SectorMap::YMAX / 2);
    selX = Cs::pi->x;
    selY = Cs::pi->y;
    ui = dm->empty(DP_MAP);
    uiText = ui->createTextField("info", 1000, 8, 248, 384, 110);
    uiText->def.wordWrap = true;
    uiText->def.multiline = true;
    uiText->def.height = 11;
    uiText->def.outlines = false;
    uiText->textColor = 0xFFFFFF;
    uiKeys = std::make_shared<flash::Listener>();
    uiKeys->onKeyDown = [this] {
        int k = flash::Stage::get()->lastKey;
        int nx = selX, ny = selY;
        if (k == flash::Key::LEFT) nx--;
        if (k == flash::Key::RIGHT) nx++;
        if (k == flash::Key::UP) ny--;
        if (k == flash::Key::DOWN) ny++;
        int reach = std::max(1, Cs::pi->engine);
        if (std::abs(nx - Cs::pi->x) <= reach && std::abs(ny - Cs::pi->y) <= reach) {
            selX = nx;
            selY = ny;
        }
        if (k == flash::Key::ENTER || k == flash::Key::SPACE) {
            startLevel(selX, selY);
            return;
        }
        if (k == flash::Key::ESCAPE) quitRequested = true;
        drawSelect();
    };
    flash::Stage::get()->keyListeners.push_back(uiKeys);
    drawSelect();
}

void Shell::closeSelect() {
    if (uiKeys) hx::remove(flash::Stage::get()->keyListeners, uiKeys);
    uiKeys = nullptr;
    if (ui) ui->removeMovieClip();
    ui = nullptr;
    uiText = nullptr;
}

void Shell::drawSelect() {
    if (!ui) return;
    ui->clear();
    // a 9x9 window of sectors around the player, coloured like the original map
    int half = 4;
    double cw = 24, ch = 24, ox = 200 - (half + 0.5) * cw, oy = 12;
    for (int dy = -half; dy <= half; dy++)
        for (int dx = -half; dx <= half; dx++) {
            int wx = Cs::pi->x + dx, wy = Cs::pi->y + dy;
            int mx = wx - smap.SX, my = wy - smap.SY;
            int col = (mx >= 0 && mx < SectorMap::XMAX && my >= 0 && my < SectorMap::YMAX) ? smap.spaceColors[mx][my] : 0;
            int zid = (mx >= 0 && mx < SectorMap::XMAX && my >= 0 && my < SectorMap::YMAX) ? smap.zoneTable[mx][my] : -1;
            double x0 = ox + (dx + half) * cw, y0 = oy + (dy + half) * ch;
            ui->lineStyle(ch - 2, col, 100);
            ui->moveTo(x0 + 1, y0 + ch / 2);
            ui->lineTo(x0 + cw - 1, y0 + ch / 2);
            if (zid >= 0) {
                int zc = zid == ZoneInfo::ASTEROBELT ? 0x777766 : ZoneInfo::list[zid].col;
                ui->lineStyle(8, zc, 100);
                ui->moveTo(x0 + cw / 2 - 4, y0 + ch / 2);
                ui->lineTo(x0 + cw / 2 + 4, y0 + ch / 2);
            }
            unsigned frame = 0;
            if (wx == Cs::pi->x && wy == Cs::pi->y) frame = 0x00FF00;
            if (wx == selX && wy == selY) frame = 0xFFFF00;
            if (frame) {
                ui->lineStyle(2, frame, 100);
                ui->moveTo(x0 + 1, y0 + 1);
                ui->lineTo(x0 + cw - 1, y0 + 1);
                ui->lineTo(x0 + cw - 1, y0 + ch - 1);
                ui->lineTo(x0 + 1, y0 + ch - 1);
                ui->lineTo(x0 + 1, y0 + 1);
            }
        }
    int mx = selX - smap.SX, my = selY - smap.SY;
    int zid = smap.zoneTable[mx][my];
    std::ostringstream s;
    s << "ALPHABOUNCE - SDL3 PORT (core game test screen)\n";
    s << "Sector [" << selX << "][" << selY << "]";
    if (zid >= 0) s << "  -  " << ZoneInfo::list[zid].name;
    s << "\nMinerai: " << Cs::pi->minerai << "   Lives: " << Cs::pi->getLife() << "   Missiles: " << Cs::pi->missile << "/"
      << Cs::pi->missileMax << "\n";
    s << "Arrows: choose sector   Enter: play   Esc: quit\n";
    s << "In game: mouse moves the pad, click launches, P pauses, Space fires missiles";
    uiText->setText(s.str());
}

void Shell::startLevel(int wx, int wy) {
    closeSelect();
    int mx = wx - smap.SX, my = wy - smap.SY;
    int zid = smap.zoneTable[mx][my];
    int col = smap.spaceColors[mx][my];
    // Api.play (offline): confirmMove(x, y, true, edited level or null)
    mcGame = dm->empty(DP_MAP);
    game = std::make_unique<Game>(mcGame, col);
    game->initLevel(wx, wy, zid, true, nullptr);
    game->initPlay();
}
