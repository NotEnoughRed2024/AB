// Level.hx - procedural level generation.
//
// Faithfulness notes (these decide the exact layout of every level):
//  * mt.OldRandom is reproduced bit-exactly (src/mt/Random.h).
//  * Haxe's Flash 8 backend evaluates call arguments LAST TO FIRST, so
//    m.translate(seed.random(a), seed.random(b)) draws b before a. Marked "args reversed" below.
//  * Grid cells distinguish undefined (never set) from null (cleared): null < 5 is true, undefined < 5 is false.
//  * The level shape comes from rasterising brushes into a 14x23 bitmap with Flash's 4x4 anti-aliasing
//    (soft BitmapData) and keeping every non-black pixel.
#include <cmath>
#include <sstream>

#include "Elements.h"
#include "Shell.h"

using hx::Cell;

Level::Level(int x, int y, int id, bool flMinerai, const std::string* level) : seed(0) {
    wx = x;
    wy = y;
    zid = id;
    flDepleted = !flMinerai;
    if (level) {
        // levels made with the in-game editor (PersistCodec) - not used by the offline port yet
        hasStruct = false;
    }
    for (auto& zone : ZoneInfo::list) {
        double dx = x - zone.pos[0], dy = y - zone.pos[1];
        distances.push_back(std::sqrt(dx * dx + dy * dy));
    }
    dst = std::sqrt((double)wx * wx + (double)wy * wy);
    ang = std::atan2((double)wy, (double)wx);
    lvl = hx::Int(std::pow(dst * 0.1, 0.5));
    ymax = hx::Int(std::min(12.0 + lvl, (double)Cs::YMAX - 6));
    initSeed();
    initProba();
}

void Level::initSeed() { seed = mt::OldRandom(wx * 10000 + wy); }

void Level::initProba() {
    proba.assign(30, 0);
    const double NEVER = 100000.0;
    auto d = [&](int z) { return z < (int)distances.size() ? distances[z] : 1e9; };
    for (int i = 0; i < 30; i++) {
        double n = NEVER;
        switch (i) {
            case PB_STEEL_BAR:
                n = std::max(5 - (dst / 200), 2.0);
                if (d(ZoneInfo::SOUPALINE) < 12) n = 1000;
                n *= std::min(d(ZoneInfo::MOLTEAR) / 50, 1.0);
                break;
            case PB_STEEL_CLOUD:
                n = 5;
                if (zid == ZoneInfo::SOUPALINE) n = NEVER;
                break;
            case PB_PUSHER:
                n = 12;
                if (dst < 6) n = NEVER;
                else if (dst < 20) n = 40;
                break;
            case PB_BOOM:
                n = 12;
                if (dst < 4) n = NEVER;
                if (wy < 0 || wx < 0) n += dst * 0.5;
                if (zid == ZoneInfo::LYCANS) n = 1;
                break;
            case PB_JUMPER:
                if (d(ZoneInfo::KARBONIS) < 100 && dst > 10) n = 40;
                break;
            case PB_STORM:
                if (dst > 30) n = std::max(60 - dst * 0.5, 4.0);
                break;
            case PB_CAGE:
                if (dst > 15) n = std::max(50 - dst, 3.0);
                break;
            case PB_GENERATOR:
                if (dst > 20) n = std::max(100 - dst * 0.5, 12.0);
                break;
            case PB_DRAGON: {
                if (wy > 10) n = 10;
                double lim = 30;
                if (d(ZoneInfo::POFIAK) < lim) n = 1 + (d(ZoneInfo::POFIAK) / lim) * 10;
                break;
            }
            case PB_MISSILE:
                if (dst > 4) n = std::min(3 + dst * 0.1, 20.0);
                if (zid == ZoneInfo::LYCANS) n = 2;
                break;
            case PB_DOOR:
                if (dst > 13 && dst < 18) n = 7;
                if (dst > 58) n = 16;
                break;
            case PB_KILL:
                if (dst > 80) n = std::max(3.0, 80 * std::fabs(mt::Num::hMod(ang - 2.504, 3.14)));
                break;
            case PB_DEATH:
                if (dst > 40) n = std::max(2.0, 100 - std::pow(dst * 10, 0.5));
                break;
            default:
                break;
        }
        proba[i] = (int)std::floor(n);
    }
}

int Level::paint(int x, int y) const { return bmpPaint ? (int)bmpPaint->getPixel(x, y) : 0x888888; }

// BUILD PALETTE
void Level::genPalette() {
    int col = 0x888888;
    std::vector<std::array<int, 6>> pal = {{55, 55, 55, 200, 200, 200}};
    if (zid >= 0) {
        col = ZoneInfo::list[zid].col;
        pal = ZoneInfo::list[zid].pal;
    }
    bmpPaint = std::make_shared<flash::BitmapData>(flash::Stage::get(), Cs::XMAX, Cs::YMAX, false, col, true);
    MC brush = Game::me->dm->attach("mcBrush", 0);
    double sc = 0.1;
    int ma = -2;
    for (int i = 0; i < 16; i++) {
        flash::Matrix m;
        m.scale(sc, sc);
        int ry = seed.random(Cs::YMAX - 2 * ma);  // args reversed
        int rx = seed.random(Cs::XMAX - 2 * ma);
        m.translate(ma + rx, ma + ry);
        const auto& pr = pal[seed.random((int)pal.size())];
        int r = pr[0] + seed.random(pr[3]);
        int g = pr[1] + seed.random(pr[4]);
        int b = pr[2] + seed.random(pr[5]);
        flash::ColorTransform ct(0, 0, 0, 0, r, g, b, 40);
        bmpPaint->draw(brush.get(), m, &ct, flash::Blend::Add);
    }
    brush->removeMovieClip();
}

void Level::genBonusTable() {
    if (zid == ZoneInfo::GRIMORN) return;
    Option::genProb(this);
    bonusTable.assign(Cs::XMAX, std::vector<int>(Cs::YMAX, -1));
    for (int x = 0; x < Cs::XMAX; x++)
        for (int y = 0; y < Cs::YMAX; y++)
            if (seed.rand() < Cs::OPTION_COEF) bonusTable[x][y] = Option::getRandomId(seed);
}

// BUILD MODEL
void Level::genModel() {
    // ITEM
    for (int id = 0; id < Cs::pi->items.size(); id++) {
        const ItemPos& item = MissionInfo::item(id);
        if (item.has && item.x == wx && item.y == wy) {
            itemId = id;
            break;
        }
    }
    if (!hasStruct) {
        int to = 0;
        while (true) {
            genPrimeModel();
            int goal = getGoal();
            if (goal > 12 && isModelOpen(goal) && goal < (lvl + 1) * 100) break;
            if (to++ > 12) {
                forceModel();
                break;
            }
        }
        // PLACE MOLS
        std::vector<int> mols;
        while (mols.size() < 20) {
            int max = 1 + seed.random(10);
            int n = el::Molecule::getRandomMolType(seed);
            for (int i = 0; i < max; i++) mols.push_back(n);
        }
        int mi = 0;
        for (int x = 0; x < Cs::XMAX; x++)
            for (int y = 0; y < Cs::YMAX; y++) {
                Cell n = model.get(x, y);
                if (n == Block::CAGE || n == Block::GENERATOR) {
                    mi = (mi + 1) % (int)mols.size();
                    model.set(x, y, Cell(n.v + mols[mi]));
                }
            }
    } else {
        genStruct();
    }
}

void Level::genPrimeModel() {
    flLure = false;
    bool flMirror = seed.random(2) == 0;
    bool flMirrorPalette = seed.random(2) == 0 && flMirror;  // && short-circuits after the call: fine
    // MASSE
    auto bmp = std::make_shared<flash::BitmapData>(flash::Stage::get(), Cs::XMAX, Cs::YMAX, false, 0, true);
    MC brush = Shell::dm->attach("mcShape", 0);
    double sc = 0.06;
    int ma = -2;
    int max = hx::Int(4 + lvl);
    for (int i = 0; i < max; i++) {
        flash::Matrix m;
        double scc = (sc * (1 + seed.rand() * 0.5));
        m.scale(scc, scc);
        int ry = seed.random(ymax - 2 * ma);  // args reversed
        int rx = seed.random(Cs::XMAX - 2 * ma);
        m.translate(ma + rx, ma + ry);
        brush->gotoAndStop(seed.random(brush->totalFrames()) + 1);
        bmp->draw(brush.get(), m);
    }
    brush->removeMovieClip();
    // FILL
    model = hx::Grid();
    model.cols.resize(Cs::XMAX);
    for (int x = 0; x < Cs::XMAX; x++)
        for (int y = 0; y < ymax; y++)
            if (bmp->getPixel(x, y) != 0) model.set(x, y, Cell(0));

    // LINE
    int lmax = lvl;
    if (zid == ZoneInfo::DOURIV || zid == ZoneInfo::GRIMORN) lmax = 0;
    for (int i = 0; i < lmax; i++) {
        auto list = getHoriLine(2 + seed.random(ymax - 2), 0.05, 0);
        for (auto& p : list) {
            Cell c = model.get(p[0], p[1]);
            if (c.lt(5)) model.set(p[0], p[1], Cell(c.num() + 1));
        }
    }
    // DIG
    while (lvl >= 0 && seed.random(2) == 0) {
        int m = 3;
        int di = seed.random(4);
        int sx = m + seed.random(Cs::XMAX - (2 * m));
        int sy = m + seed.random(ymax - (2 * m));
        while (true) {
            if (sx >= 0 && sx < Cs::XMAX && sy >= 0 && sy < ymax) {
                model.set(sx, sy, Cell::null());
                sx += Cs::DIR[di][0];
                sy += Cs::DIR[di][1];
                if (seed.random(4) == 0) di = hx::Int(mt::Num::sMod(di + (seed.random(2) * 2 - 1), 4));
            } else {
                break;
            }
        }
    }
    // BORDER
    int brd = seed.random(lvl + 1);
    if (zid == ZoneInfo::DOURIV || zid == ZoneInfo::GRIMORN || zid == ZoneInfo::CILORILE) brd = 0;
    if (brd > 0) {
        int inc = 1;
        if (brd > 2) inc++;
        if (brd > 4) inc++;
        for (int x = 0; x < Cs::XMAX; x++)
            for (int y = 0; y < ymax; y++) {
                if (model.get(x, y).lt(5)) {
                    for (auto& d : Cs::DIR) {
                        int nx = x + d[0], ny = y + d[1];
                        if (nx >= 0 && nx < Cs::XMAX && ny >= 0 && ny < ymax + 1 && model.get(nx, ny).empty()) {
                            model.set(x, y, Cell(hx::Int(std::min(model.get(x, y).num() + inc, 5.0))));
                            break;
                        }
                    }
                }
            }
    }
    // FOSSE
    while (seed.random(3) == 0) {
        auto list = getHoriLine(1 + seed.random(ymax - 2), 0.1);
        for (auto& p : list) model.set(p[0], p[1], Cell::null());
    }
    // BALL
    while (lvl >= 1 && seed.random(3) == 0) {
        int x = seed.random(Cs::XMAX);
        int y = seed.random(5);
        model.set(x, y, Cell(Block::BALL));
    }
    // REDUCTRINES
    if (wy < -40 && seed.random(4) == 0) {
        int m = hx::Int(std::min((double)seed.random(hx::Int(2 + (std::abs(wy) * 0.05))), 10.0));
        genRandom(Block::REDUC, m);
        flLure = true;
    }
    // STEEL BAR
    if (seed.random(proba[PB_STEEL_BAR]) == 0) {
        int type = -1;
        if (seed.random(3) == 0) type = seed.random(2);
        int hmin = (int)hx::round(std::max(3 - (dst / 30), 1.0));
        int hole = hmin + seed.random(2);           // args reversed: last argument first
        int sy = 3 + seed.random((ymax - 3));
        auto list = getHoriLine(sy, 0.1, type, hole);
        for (auto& p : list) model.set(p[0], p[1], Cell(Block::STEEL));
    }
    // STEEL CLOUD
    if (seed.random(proba[PB_STEEL_CLOUD]) == 0) {
        int n = hx::Int(std::min(std::sqrt(dst * 0.25), 20.0));
        int m = n + seed.random(n);
        for (int i = 0; i < m; i++) genCloud(Block::STEEL);
    }
    // BOOM
    if (seed.random(proba[PB_BOOM]) == 0) {
        int m = 0;
        while (m % 10 == 0) m += seed.random(11);
        genRandom(Block::BOOM, m, 0);
    }
    // CAGE
    if (seed.random(proba[PB_CAGE]) == 0) {
        int m = 1 + seed.random(lvl + 1);
        while (seed.random(5) == 0) m += seed.random(m);
        genRandom(Block::CAGE, m, 0);
    }
    // GENERATOR
    if (seed.random(proba[PB_GENERATOR]) == 0) {
        int m = 1 + seed.random(lvl);
        genRandom(Block::GENERATOR, m, 1);
    }
    // DOOR
    if (seed.random(proba[PB_DOOR]) == 0) {
        int m = 1 + seed.random(5);
        for (int i = 0; i < m; i++) genCloud(Block::DOOR);
        if (seed.random(8) == 0) {
            auto list = getHoriLine(1 + seed.random(ymax - 2), 0.1, -1, 2);
            for (auto& p : list) model.set(p[0], p[1], Cell(Block::DOOR));
        }
    }
    // MISSILES
    if (seed.random(proba[PB_MISSILE]) == 0) genRandom(Block::MISSILE, 1 + seed.random(6));
    // KILL
    if (seed.random(proba[PB_KILL]) == 0) genRandom(Block::KILL, 1 + seed.random(hx::Int(std::pow(dst, 0.5))));
    // LIFE
    while (seed.rand() * (std::pow(dst, 0.5) + 3) < 1) genRandom(Block::LIFE, 1 + seed.random(2));
    // DEATH
    while (seed.random(proba[PB_DEATH]) == 0) genRandom(Block::DEATH, 1 + seed.random(2));

    // ZONES MODIFS
    if (zid < 0) {
        // HORILINE SPACE
        if (dst > 11 && (dst < 12 || seed.random(10) == 0)) {
            auto list = getHoriLine(hx::Int(ymax * 0.5), 0.1);
            for (auto& p : list) model.set(p[0], p[1], Cell(Block::SPACE));
        }
        // REDUCTRINES TEST
        double d = distances[ZoneInfo::BALIXT];
        double lim = 15;
        if (d < lim) {
            int m = seed.random(hx::Int((1 - (d / lim)) * 6) + 1);
            genRandom(Block::REDUC, m);
            flLure = true;
        }
        // PUSHER
        if (seed.random(proba[PB_PUSHER]) == 0) {
            int n = hx::Int(std::min(std::sqrt(dst * 0.25), 10.0));
            int m = n + seed.random(n);
            genRandom(Block::PUSHER, m, -1, 1);
        }
        // JUMPER
        if (seed.random(proba[PB_JUMPER]) == 0) genRandom(Block::JUMPER, 6);
        // STORM
        if (seed.random(proba[PB_STORM]) == 0) {
            int ym = hx::Int(std::min(1 + dst * 0.15, (double)ymax));
            genRandom(Block::STORM, 1 + hx::Int(std::pow(seed.rand(), 3) * 16), -1, 0, ym);
            flLure = true;
        }
        // DRAGON
        if (seed.random(proba[PB_DRAGON]) == 0) {
            int m = 1 + seed.random(8);
            for (int i = 0; i < m; i++) {
                int n = seed.random(2);
                int ma = 5;
                int x = seed.random(Cs::XMAX - ma);
                int y = seed.random(ymax);
                if (n == 0) x += ma;
                if (model.get(x, y).has()) model.set(x, y, Cell(Block::DRAGON_LEFT + n));
            }
        }
        // INVERSE
        if (dst > 20 && seed.random(500) == 0) {
            genRandom(Block::SWAP, 1 + seed.random(2));
            if (seed.random(100) == 0) genRandom(Block::SWAP, 20);
        }
        // NUT
        if (dst > 30 && seed.random(300) == 0) {
            auto list = getHoriLine(1 + seed.random(ymax - 2), 0.1, -1, 2);
            for (auto& p : list) model.set(p[0], p[1], Cell(Block::NUT));
        }
        // DEATH
        if (seed.random(proba[PB_DEATH]) == 0) genRandom(Block::DEATH, 1 + seed.random(2));
        // GUARDIAN
        if (seed.random(16) == 0 && dst > 50) genRandom(Block::GUARDIAN, 1 + seed.random(6));
        // GLUE
        if (seed.random(8) == 0 && distances[ZoneInfo::SAMOSA] < 50) genRandom(Block::GLUE, 1 + seed.random(4));
    } else {
        genZoneModif();
    }

    // BONUS
    initSeed();
    int n = 1;
    int lim = 1;
    if (zid == ZoneInfo::GRIMORN) lim = 0;
    if (zid == ZoneInfo::TIBOON) lim = 0;
    if (zid == ZoneInfo::DOURIV) lim = 2;
    if (zid == ZoneInfo::EARTH) lim = 8;
    if (distances[ZoneInfo::DOURIV] < 2) lim = 3;
    while (seed.random(n++) < lim) genBonusBlock(ymax);

    // LURE
    if (flLure && dst > 130) {
        if (seed.random(4) == 0) genRandom(Block::LURE, 1 + seed.random(20));
        if (seed.random(8) == 0) {
            auto list = getHoriLine(seed.random(ymax), 0.1, 0);
            for (auto& p : list) model.set(p[0], p[1], Cell(Block::LURE));
        }
    }
    // MIRROR
    if (flMirror) {
        int mx = hx::Int(Cs::XMAX * 0.5);
        for (int x = 0; x < mx; x++) {
            int nx = Cs::XMAX - (x + 1);
            model.cols[nx] = model.cols[x];
            if (flMirrorPalette && bmpPaint) {
                for (int y = 0; y < Cs::YMAX; y++) bmpPaint->setPixel32(nx, y, bmpPaint->getPixel32(x, y));
            }
            for (int y = 0; y < ymax; y++) {
                Cell c = model.get(nx, y);
                if (c == Block::DRAGON_LEFT) model.set(nx, y, Cell(Block::DRAGON_RIGHT));
                if (c == Block::DRAGON_RIGHT) model.set(nx, y, Cell(Block::DRAGON_LEFT));
            }
        }
    }
    // START CROP
    if (dst < 8) {
        int crop = 1;
        if (dst < 2) crop++;
        if (dst < 1) crop++;
        for (int x = 0; x < Cs::XMAX; x++)
            for (int y = 0; y < ymax; y++)
                if (x < crop || x >= Cs::XMAX - crop || y < crop) model.set(x, y, Cell::null());
    }
    // START CEINTURE
    if (std::max(std::abs(wx), std::abs(wy)) == 3) {
        auto list = getHoriLine(12, 0.0);
        for (auto& p : list) model.set(p[0], p[1], Cell(1));
    }
    // OBJETS
    if (itemId >= 0 && isItemVisible()) insertItem();
}

void Level::forceModel() {
    model = hx::Grid();
    model.cols.resize(Cs::XMAX);
    for (int x = 0; x < Cs::XMAX; x++)
        for (int y = 0; y < ymax; y++) model.set(x, y, Cell(0));
}

void Level::genStruct() {
    model = hx::Grid();
    model.cols.resize(Cs::XMAX);
    for (int x = 0; x < Cs::XMAX; x++)
        for (int y = 0; y < Cs::YMAX; y++) {
            Cell n = structGrid.get(x, y);
            if (n == Block::ITEM && !isItemVisible()) n = Cell();
            if (n.has()) model.set(x, y, n);
        }
}

void Level::genZoneModif() {
    auto d = [&](int z) { return distances[z]; };
    switch (zid) {
        case ZoneInfo::MOLTEAR:
            genRandom(Block::CAGE, 10);
            break;
        case ZoneInfo::SOUPALINE: {
            auto list = getHoriLine(hx::Int(ymax * 0.5), 0.1, 0);
            for (auto& p : list) model.set(p[0], p[1], Cell(Block::SPACE));
            break;
        }
        case ZoneInfo::LYCANS:
            if (seed.random(4) == 0) {
                auto list = getHoriLine(hx::Int(ymax * 0.5), 0.1, 0);
                for (auto& p : list) model.set(p[0], p[1], Cell(Block::BOOM));
            }
            break;
        case ZoneInfo::SAMOSA:
            if (seed.random(2) == 0) genRandom(Block::GLUE, 1 + seed.random(16));
            if (seed.random(8) == 0) {
                auto list = getHoriLine(hx::Int(ymax * 0.5), 0.1, 0);
                for (auto& p : list) model.set(p[0], p[1], Cell(Block::GLUE));
            }
            break;
        case ZoneInfo::TIBOON:
            break;
        case ZoneInfo::BALIXT:
            genRandom(Block::REDUC, 2 + seed.random(12));
            flLure = true;
            break;
        case ZoneInfo::KARBONIS: {
            int m = 1;
            if (d(ZoneInfo::KARBONIS) < 1.5) m++;
            for (int i = 0; i < m; i++) {
                auto list = getHoriLine(4 + seed.random(ymax - 8), 0.15);
                for (auto& p : list) model.set(p[0], p[1], Cell(Block::JUMPER));
            }
            break;
        }
        case ZoneInfo::SPIGNYSOS:
            genRandom(Block::CAGE + el::Molecule::LIGHTER, 1 + seed.random(3));
            if (seed.random(5) == 0) genRandom(Block::CAGE + el::Molecule::SPACER, 1 + seed.random(5));
            break;
        case ZoneInfo::POFIAK:
            genRandom(Block::CAGE + el::Molecule::BUILDER, 1 + seed.random(2));
            if (seed.random(3) == 0) genRandom(Block::INSECT, 1 + seed.random(4));
            break;
        case ZoneInfo::SENEGARDE: {
            double c = 1 - d(ZoneInfo::SENEGARDE) / ZoneInfo::list[ZoneInfo::SENEGARDE].pos[2];
            genRandom(Block::GENERATOR, hx::Int(1 + c * 8));
            break;
        }
        case ZoneInfo::DOURIV:
            for (int i = 0; i < 3; i++) {
                auto list = getHoriLine(2 + i * 5, 0.2, 0);
                for (auto& p : list) model.set(p[0], p[1], Cell(4));
            }
            if (seed.random(4) == 0) genRandom(Block::INSECT, 3 + seed.random(12));
            break;
        case ZoneInfo::GRIMORN:
            for (int i = 0; i < 20; i++) genCloud(Block::STEEL);
            break;
        case ZoneInfo::DTRITUS:
            genRandom(Block::INSECT, 1 + seed.random(24));
            break;
        case ZoneInfo::NALIKORS:
            genRandom(Block::LIFE, 1);
            genRandom(Block::DEATH, 1 + seed.random(2));
            if (seed.random(20) == 0) {
                auto list = getHoriLine(1 + seed.random(ymax - 2), 0.1, -1, 2);
                for (auto& p : list) model.set(p[0], p[1], Cell(Block::DEATH));
            }
            break;
        case ZoneInfo::HOLOVAN:
            if (seed.random(2) == 0) {
                auto list = getHoriLine(1 + seed.random(ymax - 2), 0.1, -1, 2);
                for (auto& p : list) model.set(p[0], p[1], Cell(Block::KILL));
            }
            while (seed.random(2) == 0) genRandom(Block::KILL, 1 + seed.random(6));
            break;
        case ZoneInfo::KHORLAN: {
            int n = 1;
            while (seed.rand() * n < 1) {
                auto list = getHoriLine(1 + seed.random(ymax - 2), 0.1, -1, 2);
                for (auto& p : list) model.set(p[0], p[1], Cell(Block::NUT));
                n *= 2;
            }
            if (seed.random(2) == 0) genRandom(Block::CAGE + el::Molecule::ARMORER, 1 + seed.random(3));
            break;
        }
        case ZoneInfo::CILORILE:
            genRandom(Block::GUARDIAN, 1 + seed.random(16));
            break;
        case ZoneInfo::TARCITURNE: {
            int n = 1;
            while (seed.rand() * n < 3) {
                genCloud(Block::STEEL);
                n++;
            }
            break;
        }
        case ZoneInfo::CHAGARINA:
            if (seed.random(2) == 0) {
                auto list = getHoriLine(1 + seed.random(ymax - 2), 0.1, -1, 6);
                for (auto& p : list) model.set(p[0], p[1], Cell(Block::DEATH));
            }
            break;
        case ZoneInfo::VOLCER: {
            int m = hx::Int((8 - d(ZoneInfo::VOLCER)) * 2);
            genRandom(Block::REDUC, m);
            genRandom(Block::KILL, m);
            if (seed.random(2) == 0) genRandom(Block::LURE, m);
            break;
        }
        case ZoneInfo::BALMANCH: {
            int m = 10 + seed.random(10);
            for (int i = 0; i < m; i++) genCloud(Block::NUT);
            int m2 = 1 + seed.random(4);
            for (int i = 0; i < 2; i++) {
                int sens = i * 2 - 1;
                for (int n = 0; n < m2; n++) {
                    int x = i * (Cs::XMAX - 1) - sens * seed.random(4);
                    int y = seed.random(ymax);
                    model.set(x, y, Cell(i == 0 ? Block::DRAGON_RIGHT : Block::DRAGON_LEFT));
                }
            }
            searchAndReplace(Block::STEEL, 4);
            break;
        }
        case ZoneInfo::FOLKET: {
            int m = hx::Int((3 - d(ZoneInfo::FOLKET)) * 3);
            for (int i = 0; i < m; i++) genCloud(Block::REDUC);
            searchAndReplace(Block::STEEL, Block::SPACE);
            searchAndReplace(Block::BONUS + 1, Block::BONUS + 1);
            searchAndReplace(Block::BONUS + 2, Block::BONUS + 1);
            searchAndReplace(Block::BONUS + 3, Block::BONUS + 1);
            searchAndReplace(Block::BONUS, Block::BONUS + 1);
            for (int i = 1; i < 6; i++) searchAndReplace(i, Block::SPACE);
            break;
        }
        case ZoneInfo::EARTH:
            for (int x = 0; x < Cs::XMAX; x++)
                for (int y = 0; y < Cs::YMAX; y++)
                    if (model.get(x, y).has()) model.set(x, y, Cell(0));
            break;
    }
}

// BUILD TOOLS
void Level::genBonusBlock(int ym) {
    int max = hx::Int(std::min(2.0 + lvl, 4.0));
    int mx = 1 + seed.random(max);
    int my = 1 + seed.random(max);
    int sx = seed.random(Cs::XMAX - mx);
    int sy = seed.random(ym - my);
    int po = 0;
    double bluePower = std::max(2.5 - dst * 0.004, 1.0);
    double pinkPower = std::max(3.5 - dst * 0.004, 2.0);
    if (seed.random(hx::Int(std::pow(mx + my + 1, bluePower))) == 0) po = 1;
    if (seed.random(hx::Int(std::pow(mx + my + 1, pinkPower))) == 0) po = 2;
    int type = 10 + po;
    for (int x = 0; x < mx; x++)
        for (int y = 0; y < my; y++) model.set(sx + x, sy + y, Cell(type));
}

std::vector<std::array<int, 2>> Level::getHoriLine(int sy, double turnCoef, int type, int hole) {
    // type: -1 over, 0 fill (existing cells only), 1 behind (empty cells only)
    int hx_ = -1000;
    bool hasHole = hole >= 0;
    if (hasHole) hx_ = seed.random(Cs::XMAX - hole);
    std::vector<std::array<int, 2>> list;
    int y = sy;
    auto consider = [&](int x, int yy) {
        if (type < 0) {
            list.push_back({x, yy});
        } else {
            bool prec = model.get(x, yy).has();
            if (prec) {
                if (type == 0) list.push_back({x, yy});
            } else {
                if (type == 1) list.push_back({x, yy});
            }
        }
    };
    for (int x = 0; x < Cs::XMAX; x++) {
        if (!hasHole || (x < hx_ || x >= hx_ + hole)) {
            consider(x, y);
            if (seed.rand() < turnCoef) {
                int sens = seed.random(2) * 2 - 1;
                int max = 1 + seed.random(8);
                for (int i = 0; i < max; i++) {
                    y += sens;
                    if (y < ymax && y >= 0) {
                        consider(x, y);
                    } else {
                        y -= sens;
                        break;
                    }
                }
            }
        }
    }
    return list;
}

void Level::genRandom(int type, int max, int drawType, int ma, int ym) {
    if (ym < 0) ym = ymax;
    for (int i = 0; i < max; i++) {
        int x = ma + seed.random(Cs::XMAX - 2 * ma);
        int y = ma + seed.random(ym - 2 * ma);
        Cell n = model.get(x, y);
        switch (drawType) {
            case 0:
                if (n.has()) model.set(x, y, Cell(type));
                break;
            case 1:
                if (n.empty()) model.set(x, y, Cell(type));
                break;
            default:
                model.set(x, y, Cell(type));
        }
    }
}

void Level::genCloud(int type) {
    int x = seed.random(Cs::XMAX);
    int y = seed.random(ymax);
    const int* d = Cs::DIR[seed.random(4)];
    int lmax = 2 + seed.random(4);
    for (int n = 0; n < lmax; n++) {
        int nx = x + d[0] * n, ny = y + d[1] * n;
        if (isOut(nx, ny)) break;
        model.set(nx, ny, Cell(type));
    }
}

void Level::insertItem() {
    int ma = 4;
    int x = ma + seed.random(Cs::XMAX - 2 * ma);
    int y = seed.random(3);
    model.set(x, y, Cell(Block::ITEM));
}

bool Level::isItemVisible() const {
    int n = Cs::pi->items.get(itemId);
    return n == MissionInfo::VISIBLE || n == MissionInfo::SURPRISE || flEdit;
}

void Level::searchAndReplace(int o, int n) {
    for (int x = 0; x < Cs::XMAX; x++)
        for (int y = 0; y < Cs::YMAX; y++)
            if (model.get(x, y) == o) model.set(x, y, Cell(n));
}

flash::Point Level::getRandomBlock() {
    int to = 0;
    while (true) {
        int x = seed.random(Cs::XMAX);
        int y = seed.random(ymax);
        double id = model.get(x, y).num();
        if (id > 0 && id != Block::MINE) return {(double)x, (double)y};
        if (to++ > 100) break;
    }
    return {1, 1};
}

flash::Point Level::getMineCoord() {
    int to = 0;
    while (true) {
        int x = seed.random(Cs::XMAX);
        int y = seed.random(ymax);
        if (model.get(x, y) == 0) return {(double)x, (double)y};
        if (to++ > 100) break;
    }
    return getRandomBlock();
}

void Level::addMine() {
    flash::Point p = getMineCoord();
    model.set((int)p.x, (int)p.y, Cell(Block::MINE));
}

std::string Level::getString() const {
    std::ostringstream s;
    for (int x = 0; x < Cs::XMAX; x++)
        for (int y = 0; y < Cs::YMAX; y++) {
            Cell n = model.get(x, y);
            s << (n.has() ? (int)n.v + 1 : 0);
            if (!(x == Cs::XMAX - 1 && y == Cs::YMAX - 1)) s << ",";
        }
    return s.str();
}

// BUILD TEST
bool Level::isModelOpen(int obj) {
    int sx = -1;
    int sy = ymax - 1;
    std::vector<std::vector<bool>> grid(Cs::XMAX, std::vector<bool>(Cs::YMAX + 2, false));
    for (int x = 0; x < Cs::XMAX; x++)
        if (sx < 0 && Block::isSoft(model.get(x, sy))) sx = x;
    if (sx < 0) return false;
    int n = getPath(sx, sy, grid, 0);
    return n == obj;
}

int Level::getPath(int x, int y, std::vector<std::vector<bool>>& grid, int depth) {
    if (depth == 240) return 0;
    int n = 0;
    grid[x][y] = true;
    if (model.get(x, y) == 0) n++;
    for (auto& d : Cs::DIR) {
        int nx = x + d[0], ny = y + d[1];
        bool visited = (nx >= 0 && nx < Cs::XMAX && ny >= 0 && ny < (int)grid[nx].size()) ? grid[nx][ny] : false;
        if (!visited && nx >= 0 && nx < Cs::XMAX && ny >= 0 && ny < ymax + 1) {
            if (Block::isSoft(model.get(x, y))) n += getPath(nx, ny, grid, depth + 1);
        }
    }
    return n;
}

int Level::getGoal() const {
    int obj = 0;
    for (int x = 0; x < Cs::XMAX; x++)
        for (int y = 0; y < Cs::YMAX; y++)
            if (model.get(x, y) == 0) obj++;
    return obj;
}

int Level::getMineraiTotal() const {
    int m = 0;
    for (int x = 0; x < Cs::XMAX; x++)
        for (int y = 0; y < ymax; y++) {
            Cell n = model.get(x, y);
            if (n == Block::BONUS) m += Block::MIN_GREEN;
            if (n == Block::BONUS + 1) m += Block::MIN_BLUE;
            if (n == Block::BONUS + 2) m += Block::MIN_PINK;
        }
    return m;
}
