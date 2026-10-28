// Module.hx, Game.hx
#include "Game.h"

#include <cmath>

#include "Elements.h"
#include "Shell.h"

namespace Col = mt::Col;
namespace Filt = mt::Filt;
namespace Num = mt::Num;

// ------------------------------------------------------------------ Module

Module::Module(MC mc) : root(std::move(mc)) {
    dm = std::make_unique<mt::DepthManager>(root);
    initMouseListener();
    flView = false;
}

Module::~Module() = default;

void Module::initLevel(int x, int y, int zid, bool flMinerai, const std::string* lvl) {
    wx = x;
    wy = y;
    level = std::make_unique<Level>(x, y, zid, flMinerai, lvl);
}

void Module::initPlay() { flView = true; }

void Module::update() {
    if (!hx::isNull(pauseCoef)) updatePause();
}

void Module::updateSprites() {
    auto list = Sprite::spriteList;
    for (auto* sp : list)
        if (!sp->dead) sp->update();
}

flash::BMP Module::getBmpBg(int col) {
    auto bmpBg = std::make_shared<flash::BitmapData>(flash::Stage::get(), Cs::mcw, Cs::mch, false, col);
    mt::Random seed(wx * 1000 + wy);
    MC brushLight = dm->attach("mcLuz", 0);
    double sc = 6;
    for (int i = 0; i < 6; i++) {
        flash::Matrix m;
        double sy = (0.5 + seed.rand()) * sc;  // args reversed
        double sx = (0.5 + seed.rand()) * sc;
        m.scale(sx, sy);
        int ty = seed.random(Cs::mch);  // args reversed
        int tx = seed.random(Cs::mcw);
        m.translate(tx, ty);
        int bi = 5, ri = 50;
        Col::RGB o;
        o.r = bi + seed.random(ri);  // object literal fields are evaluated in order
        o.g = bi + seed.random(ri);
        o.b = bi + seed.random(ri);
        if (auto s = brushLight->smc()) {
            Col::setPercentColor(s.get(), 100, Col::objToCol(o));
            s->alpha = 50;
        }
        flash::Blend bl = (i % 2 == 0) ? flash::Blend::Subtract : flash::Blend::Add;
        bmpBg->draw(brushLight.get(), m, nullptr, bl);
    }
    brushLight->removeMovieClip();
    MC brushStar = dm->attach("mcStar", 0);
    for (int i = 0; i < 100; i++) {
        flash::Matrix m;
        double s = 0.2 + seed.rand() * 0.3;
        m.scale(s, s);
        double ty = seed.rand() * Cs::mch;  // args reversed
        double tx = seed.rand() * Cs::mcw;
        m.translate(tx, ty);
        bmpBg->draw(brushStar.get(), m, nullptr, flash::Blend::Add);
    }
    brushStar->removeMovieClip();
    return bmpBg;
}

void Module::initEnding(bool fl) {
    flVictory = fl;
    victoryTimer = 0;
    flEndConnect = false;
}

void Module::updateEnding() {
    victoryTimer += mt::Timer::tmod;
    if (victoryTimer > 12) {
        if (!mcEnding) {
            mcEnding = dm->attach("mcEnding", 20);
            mcEnding->y = Cs::mch * 0.5;
            mcEnding->yscale = 0;
            if (auto f = mcEnding->field("field")) Filt::glow(f.get(), 10, 1, 0xFFFFFF);
            mcEnding->blendMode = flash::Blend::Add;
        }
        if (mcEnding->yscale < 100) {
            mcEnding->yscale += (100 - mcEnding->yscale) * 0.5;
            if (mcEnding->yscale >= 98) mcEnding->yscale = 100;
        }
        std::string baseStr = flVictory ? "MISSION OK" : "GAME OVER";
        int n = hx::Int((victoryTimer - 8) * 0.5);
        std::string str = baseStr.substr(0, std::max(0, std::min(n, (int)baseStr.size())));
        if (std::fmod(victoryTimer, 4) < 2) str += "_";
        if (auto f = mcEnding->field("field")) f->setText(str);
    }
    if (!flEndConnect && victoryTimer > 60) {
        flEndConnect = true;
        Shell::get()->initConnexion();
        endGame();
    }
}

void Module::togglePause() {
    if (pauseCount++ > 31 || Cs::DEMO) return;
    flPause = !flPause;
    if (flPause) {
        pauseCoef = 0;
        if (!hasCaps) {
            mcPause = Shell::dm->attach("mcBgCaps", 10);
            mcPause->yscale = 0;
            mcPause->y = Cs::mch * 0.5;
            capsCoef = 0;
            caps.clear();
            hasCaps = true;
            int max = (int)specials.size();
            for (int i = 0; i < max; i++) {
                Special& spec = specials[i];
                Special c;
                c.mc = Shell::dm->attach("mcMenuCaps", 10);
                c.cx = max == 1 ? 0.5 : ((double)i / (max - 1)) * 2 - 1;
                c.mc->x = Cs::mcw * 0.5;
                c.mc->y = Cs::mch * 0.5;
                c.mc->gotoAndStop(spec.id + 1);
                c.id = spec.id;
                caps.push_back(c);
            }
        }
        int type = caps.empty() ? 1 : 0;
        mcPause->gotoAndStop(type + 1);
        if (auto f = mcPause->field("fieldDesc")) f->setText(Text::at("PAUSE_TEXT", type));
    } else {
        if (hx::isNull(pauseCoef)) pauseCoef = 0;
    }
}

void Module::updatePause() {
    double sens = flPause ? 1 : -1;
    pauseCoef = Num::mm(0, pauseCoef + 0.15 * sens * mt::Timer::tmod, 1);
    const double base[20] = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0};
    double r = 0.3, g = 0.5, b = 0.1, a = 30;
    const double grey[20] = {r, g, b, 0, a, r, g, b, 0, a, r, g, b, 0, a, 0, 0, 0, 1, 0};
    double matrix[20];
    for (int i = 0; i < 20; i++) matrix[i] = base[i] * (1 - pauseCoef) + grey[i] * pauseCoef;
    root->filters = {flash::Filter::colorMatrix(matrix)};
    double cc = (1 - std::cos(pauseCoef * 3.14)) * 0.5;
    TF fieldName = mcPause ? mcPause->field("fieldName") : nullptr;
    for (auto& c : caps) {
        MC mc = c.mc;
        mc->x = Cs::mcw * 0.5 + c.cx * cc * ((caps.size() - 1) * 30);
        mc->rotation = (1 - cc) * 180;
        mc->xscale = mc->yscale = 20 + 80 * cc;
        if (pauseCoef == 1 && !mc->onPress) {
            int id = c.id;
            flash::MovieClip* raw = mc.get();
            mc->onRollOver = [this, raw, id, fieldName]() {
                Filt::glow(raw, 2, 4, 0xFFFFFF);
                Filt::glow(raw, 10, 1, 0xFFFFFF);
                raw->blendMode = flash::Blend::Add;
                if (fieldName) {
                    fieldName->setText(Text::at("CAPS_NAME", id));
                    fieldName->alpha = 100;
                }
                flFadeName = 0;
            };
            mc->onRollOut = [this, raw]() {
                raw->filters.clear();
                raw->blendMode = flash::Blend::Normal;
                flFadeName = 1;
            };
            mc->onDragOver = mc->onRollOver;
            mc->onDragOut = mc->onRollOver;
            mc->onPress = [this, id]() { useSpecial(id); };
            mc->useHandCursor = true;
        }
        if (pauseCoef < 1 && mc->onPress) {
            if (mc->onRollOut) mc->onRollOut();
            mc->onRollOver = nullptr;
            mc->onRollOut = nullptr;
            mc->onDragOver = nullptr;
            mc->onDragOut = nullptr;
            mc->onPress = nullptr;
            mc->useHandCursor = false;
        }
    }
    if (mcPause) {
        mcPause->yscale = cc * 100;
        if (flFadeName && fieldName && fieldName->alpha > 0) fieldName->alpha -= 10 * mt::Timer::tmod;
        double speed = 3;
        if (auto b0 = mcPause->clip("b0")) b0->x = Num::sMod(b0->x + speed, 110);
        if (auto b1 = mcPause->clip("b1")) b1->x = Num::sMod(b1->x - speed, 110);
    }
    if (pauseCoef == 0) {
        pauseCoef = hx::NUL;
        root->filters.clear();
        for (auto& c : caps) c.mc->removeMovieClip();
        caps.clear();
        hasCaps = false;
        if (mcPause) mcPause->removeMovieClip();
        mcPause = nullptr;
    }
}

void Module::initMouseListener() {
    ml = std::make_shared<flash::Listener>();
    ml->onMouseDown = [this] { mouseDown(); };
    ml->onMouseUp = [this] { mouseUp(); };
    ml->onMouseMove = [this] { mouseMove(); };
    flash::Stage::get()->mouseListeners.push_back(ml);
}

void Module::mouseDown() {
    flPress = true;
    flClick = true;
}

void Module::mouseUp() { flPress = false; }

void Module::mouseMove() {
    flash::Point p = root->mouse();
    Cs::MX = hx::Int(p.x);
    Cs::MY = hx::Int(p.y);
}

void Module::kill() {
    auto* st = flash::Stage::get();
    hx::remove(st->mouseListeners, ml);
    if (kl) hx::remove(st->keyListeners, kl);
    root->removeMovieClip();
    Shell::showMouse();
    Shell::dm->clear(10);
    Shell::dm->clear(11);
}

// ------------------------------------------------------------------ Game

bool Game::FL_DEBUG = false;
bool Game::PLAY_AUTO = false;
Game* Game::me = nullptr;

Game::Game(MC mc, int col) : Module(std::move(mc)) {
    me = this;
    base = dm->empty(DP_BLOCK);
    Filt::glow(base.get(), 4, 2, 0xFFFFFF);
    bdm = std::make_unique<mt::DepthManager>(base);
    spaceColor = col;
    flPause = false;
    flSwap = false;
    flItemFall = false;
    monsterGrid.assign(Cs::XMAX, std::vector<std::vector<el::Molecule*>>(Cs::YMAX));
    min = 0;
    accTimer = 0;
    pauseCount = 0;
    difficulty = Cs::pi->gotItem(MissionInfo::MODE_DIF) ? 2 : 1;
    life = Cs::pi->getLife();
    missile = Cs::pi->missile;
    missileType = Cs::pi->getMissileType();
    missileCadence = Cs::pi->getMissileCadence();
    missileTurnSpeed = Cs::pi->getMissileTurnSpeed();
    newPad();
    if (Cs::PREF_GFX > 0.5) initPlasma();
    initKeyListener();
    initInter();
    initCursor();
    mouseMove();
    if (Cs::pi->missileMax > 0 && Cs::pi->shopItems.get(ShopInfo::MISSILE_GENERATOR) == 1) incMissile(1);
}

Game::~Game() {
    if (me == this) me = nullptr;
}

// BG
void Game::initBg() {
    bg = dm->empty(0);
    bmpBg = getBmpBg(spaceColor);
    bg->attachBitmap(bmpBg, 0);
    int zid = level->zid;
    if (zid >= 0 && Shell::mcPlanet) drawPlanet(zid, Shell::mcPlanet.get());
    if (Cs::pi->shopItems.get(ShopInfo::SUNGLASSES) == 1) {
        Col::RGB o = Col::colToObj(spaceColor);
        double br = std::max(std::max(o.r * 0.7, (double)o.g), o.b * 0.4);
        if (br > 100) {
            mcSunglasses = mt::DepthManager(bg).attach("mcDarkScreen", 1);
            mcSunglasses->blendMode = flash::Blend::Subtract;
        }
    }
    bg->cacheAsBitmap = true;
}

void Game::drawPlanet(int zid, flash::MovieClip* mc) {
    if (auto s = mc->smc()) s->gotoAndStop(zid + 1);
    const Zone& zi = ZoneInfo::list[zid];
    double scx = zi.pos[2] * 2 * Cs::mcw * 0.01;
    double scy = zi.pos[2] * 2 * Cs::mch * 0.01;
    flash::Matrix m;
    m.scale(scx, scy);
    m.translate((zi.pos[0] - wx) * Cs::mcw, (zi.pos[1] - wy) * Cs::mch);
    flash::ColorTransform ct(1, 1, 1, 1, 0, 0, 0, 0);
    auto* st = flash::Stage::get();
    // BASE
    auto bmp = std::make_shared<flash::BitmapData>(st, Cs::mcw, Cs::mch, true, 0x00000000);
    bmp->draw(mc, m, &ct);
    // BLUR
    bmp->applyBlur(8, 8);
    // TEXTURE
    auto text = std::make_shared<flash::BitmapData>(st, Cs::mcw, Cs::mch, false, 0);
    mt::OldRandom seed(level->wx * 10000 + level->wy);
    text->perlinNoise(Cs::mcw, Cs::mch, 4, seed.random(1000), false, false, 7, true);
    flash::ColorTransform ct2(1, 1, 1, 0.4, 0, 0, 0, 0);
    bmp->draw(text.get(), flash::Matrix(), &ct2, flash::Blend::Add);
    // DRAW
    double inc = -30;
    flash::ColorTransform ct3(1, 1, 1, 1, inc, inc, inc, 0);
    bmpBg->draw(bmp.get(), flash::Matrix(), &ct3);
}

// INTER
void Game::initInter() {
    if (Cs::DEMO) return;
    mcInter = dm->empty(DP_INTER);
    interDm = std::make_unique<mt::DepthManager>(mcInter);
    mcInter->y = Cs::mch;
    initSpecials();
    placeSpecials();
    MC mis = interDm->attach("mcInterMissile", 0);
    mis->name = "mis";
    if (auto s = mis->smc()) s->gotoAndStop(missileType + 1);
    Filt::glow(mcInter.get(), 2, 4, 0);
    incMissile(0);
    lives.clear();
    for (int n = 0; n < life; n++) newLife(n);
}

void Game::newLife(int n, bool flSpark) {
    MC mc = interDm->attach("mcLife", 0);
    mc->x = 16 + n * 30;
    mc->y = -7;
    lives.push_back(mc);
    if (flSpark) {
        for (int i = 0; i < 32; i++) {
            auto* p = new Phys(dm->attach("partSpark", DP_INTER));
            p->x = mcInter->x + mc->x + (hx::random() * 2 - 1) * 10;
            p->y = mcInter->y + mc->y + hx::random() * 3;
            p->weight = -(0.05 + hx::random() * 0.2);
            p->timer = 10 + hx::random() * 10;
            p->sleep = i * 0.5;
            p->fadeType = 0;
            p->setScale(100 + hx::random() * 150);
            p->root->blendMode = flash::Blend::Add;
        }
    }
}

void Game::incMissile(int n) {
    missile += n;
    if (missile > Cs::pi->missileMax) missile = Cs::pi->missileMax;
    if (!mcInter) return;
    MC mis = mcInter->clip("mis");
    if (!mis) return;
    if (auto f = mis->field("field")) f->setText(std::to_string(missile) + "/" + std::to_string(Cs::pi->missileMax));
    mis->visible = missile > 0;
    mis->x = Cs::mcw - specials.size() * 12.0;
}

void Game::incMinerai(int n) {
    min += n;
    if (!Cs::DEMO) Shell::hideMouse();
}

void Game::updateInter() {}

// CURSOR
void Game::initCursor() {
    if (Cs::DEMO) return;
    mcCursor = Shell::dm->attach("mcCursor", 11);
    mcCursor->alpha = 50;
    Shell::hideMouse();
}

void Game::updateCursor() {
    if (Cs::DEMO || !mcCursor) return;
    mcCursor->x = Pad::getPadX();
    mcCursor->y = Cs::MY;
    double m = 0;
    if (!flPause && (mcCursor->x > Cs::mcw + m || mcCursor->x < -m || mcCursor->y > Cs::mch + m || mcCursor->y < -m)) {
        if (!mcWarning && Cs::PREF_BOOLS[2]) {
            mcWarning = dm->attach("mcWarning", DP_INTER);
            if (auto f = mcWarning->field("field")) f->setText(Text::str("WARNING_ZONE"));
        }
    } else if (mcWarning) {
        mcWarning->removeMovieClip();
        mcWarning = nullptr;
    }
}

// UPDATE
void Game::update() {
    Module::update();
    if (mcFlash) updateFlash();
    updateCursor();
    updateTitle();
    if (!hx::isNull(pauseCoef)) return;
    if (pad && pad->flStop) {
        if (hx::isNull(timeCoef)) timeCoef = 1;
        timeCoef = std::max(timeCoef - 0.3 * mt::Timer::tmod, 0.1);
    } else if (!hx::isNull(timeCoef)) {
        timeCoef = std::min(timeCoef + 0.08 * mt::Timer::tmod, 1.0);
        if (timeCoef == 1) timeCoef = hx::NUL;
    }
    if (!hx::isNull(timeCoef)) mt::Timer::tmod = timeCoef;
    switch (step) {
        case Play: updatePlay(); break;
        case Ending: updateEnding(); break;
    }
    if (!me) return;  // the level ended and was torn down
    updatePlasma();
    updateInter();
    flClick = false;
    if (!hx::isNull(respawnTimer)) {
        respawnTimer -= mt::Timer::tmod;
        if (respawnTimer <= 0) {
            newPad();
            respawnTimer = hx::NUL;
        }
    }
    if (!hx::isNull(shake)) {
        if (std::fabs(shake) < 1) shake = 0;
        base->y = shake;
        shake *= -0.75;
        base->filters.clear();
        if (shake == 0) shake = hx::NUL;
        else Filt::blur(base.get(), 0, std::fabs(shake));
    }
    mt::Plasma::updateAll();
}

// PLAY
void Game::initPlay() {
    Module::initPlay();
    step = Play;
    levelTimer = 0;
    autoLaunchTimer = 0;
}

void Game::updatePlay() {
    if (!flFirstBall) {
        levelTimer += mt::Timer::tmod;
        autoLaunchTimer += mt::Timer::tmod;
    }
    if (autoLaunchTimer > 200) {
        autoLaunchTimer = 0;
        for (auto* b : balls) b->unglue();
    }
    double mult = 1.0;
    if (level->lvl >= 10) mult = level->lvl * 0.1;
    mult *= difficulty;
    if (!flFirstBall) accTimer += mult * mt::Timer::tmod;
    if (accTimer > Cs::TEMPO) {
        for (auto* b : balls)
            if (b->speed < (7 + level->dst * 0.35) * difficulty) b->setSpeed(b->speed + 0.5);
        accTimer = 0;
    }
    if (!flFirstBall) inactiveTimer += mt::Timer::tmod;
    double timer = 200 + block * 40 - inactiveTimer;
    if (mcCursor)
        if (auto s = mcCursor->smc()) {
            if (timer < 200) s->gotoAndStop(hx::Int((1 - timer / 200) * 160) + 1);
            else s->gotoAndStop(1);
        }
    if (timer < 0) {
        if (pad) pad->initCharge();
        inactiveTimer = 0;
    }
    updateSprites();
    auto evs = events;
    for (auto* e : evs)
        if (!e->dead) e->update();
    auto crs = crawlers;
    for (auto* c : crs)
        if (c->update) c->update();
}

void Game::removeBlock() {
    inactiveTimer = 0;
    block--;
    if (block == 0) initEnding(true);
}

void Game::cleanAll() {
    while (!balls.empty()) balls.back()->kill();
    while (!options.empty()) options.back()->kill();
    while (!events.empty()) events[0]->kill();
    crawlers.clear();
}

// VICTORY
void Game::initEnding(bool fv) {
    Module::initEnding(fv);
    if (Cs::DEMO) return;
    step = Ending;
    for (auto* b : balls) b->flImmortal = true;
    for (auto& mc : titles) mc->removeMovieClip();
}

void Game::updateEnding() {
    Module::updateEnding();
    if (flEndConnect) return;
    if (flItemFall) victoryTimer = 0;
    double lim = 50;
    if (victoryTimer > lim && pad) pad->y += (victoryTimer - lim) * 2;
    updateSprites();
    auto evs = events;
    for (auto* e : evs)
        if (!e->dead) e->update();
    auto crs = crawlers;
    for (auto* c : crs)
        if (c->update) c->update();
}

void Game::endGame() {
    cleanAll();
    int item = -1;
    if (flItemCollected) item = level->itemId;
    if (flVictory && level->itemId >= 0 && Cs::pi->items.get(level->itemId) == MissionInfo::TRIGGER)
        item = level->itemId;
    Shell::get()->endGame(wx, wy, flVictory, min, missile, item, specialSpent);
}

// PAD
void Game::newPad() {
    pad = new Pad(dm->attach("mcPad", DP_PAD));
    if (!hx::isNull(respawnTimer)) {
        if (!lives.empty()) {
            lives.back()->play();
            lives.pop_back();
        }
        pad->init();
        life -= 1;
        pad->y = 500;
    }
}

void Game::killPad() {
    if (!pad) return;
    pad->explode(dm->empty(DP_PARTS));
    pad = nullptr;
    while (!balls.empty()) balls.back()->kill();
    if (life > 0) respawnTimer = 30;
    else initEnding(false);
}

// OPTIONS
void Game::newOption(int t, double x, double y) {
    if (hx::isNull(x)) x = pad ? pad->x : 0;
    if (hx::isNull(y)) y = pad ? pad->y - 60 : 0;
    auto* opt = new Option(dm->attach("mcOption", DP_OPTION));
    opt->x = x;
    opt->y = y;
    opt->setType(t);
}

void Game::getOption(int id) {
    switch (id) {
        case 0: pad->setType(Cs::PAD_AIMANT); break;
        case 1:
            for (auto* bl : blocks)
                if (bl->type < 5) bl->setLife(bl->life + 1);
            break;
        case 2: pad->setType(Cs::PAD_GLUE); break;
        case 3:
            pad->setRay(std::max(pad->ray - 15, (double)Pad::SIDE + 1));
            pad->powerUp();
            break;
        case 4:
            pad->setRay(std::min(pad->ray + 15, 80.0));
            pad->powerUp();
            break;
        case 5:
            for (auto* b : std::vector<el::Ball*>(balls)) b->setType(Cs::BALL_FIRE);
            break;
        case 6:
            for (auto* b : std::vector<el::Ball*>(balls)) b->setType(Cs::BALL_ICE);
            break;
        case 7:
            for (auto* b : std::vector<el::Ball*>(balls)) b->setType(Cs::BALL_HALO);
            break;
        case 8: new ev::Indigestion(); break;
        case 9: pad->initCharge(); break;
        case 10:
            for (auto* b : std::vector<el::Ball*>(balls)) b->setType(Cs::BALL_KAMIKAZE);
            break;
        case 11: pad->setType(Cs::PAD_LASER); break;
        case 12: {
            auto list = balls;
            for (auto* b : list) {
                if ((int)balls.size() >= Cs::MAX_BALL) break;
                if (b->type != Cs::BALL_SHADE) {
                    el::Ball* ball = b->clone();
                    double a = std::atan2(b->vy, b->vx);
                    double ma = 0.15;
                    ball->vx = std::cos(a + ma) * ball->speed;
                    ball->vy = std::sin(a + ma) * ball->speed;
                    b->vx = std::cos(a - ma) * b->speed;
                    b->vy = std::sin(a - ma) * b->speed;
                }
            }
            break;
        }
        case 13: {
            el::Ball* b = pad->initStartBall();
            b->fxLight();
            break;
        }
        case 14: new ev::Ouverture(); break;
        case 15:
            missile = Cs::pi->missileMax;
            incMissile(0);
            break;
        case 16: new ev::Quasar(); break;
        case 17: pad->setType(Cs::PAD_GENERATOR); break;
        case 18:
            newLife(life, true);
            life += 1;
            break;
        case 19: pad->setType(Cs::PAD_TIME); break;
        case 20: new ev::UltraViolet(); break;
        case 21:
            for (auto* b : std::vector<el::Ball*>(balls)) b->setType(Cs::BALL_VOLT);
            break;
        case 22:
            for (auto* b : std::vector<el::Ball*>(balls)) b->setType(Cs::BALL_DRUNK);
            break;
        case 23:
            for (auto* b : balls) b->setSpeed(std::max(b->speed - 5, 3.0));
            break;
        case 24:
            for (auto* b : std::vector<el::Ball*>(balls)) b->setType(Cs::BALL_YOYO);
            break;
        case 25:
            for (auto* b : balls) b->setSpeed(b->speed + 5);
            break;
        case 26: incMissile(1); break;
    }
    newTitle(Text::at("OPTION_NAMES", id), Option::getCol(id));
}

// SPECIAL
void Game::initSpecials() {
    for (auto& s : specials) s.mc->removeMovieClip();
    specials.clear();
    const int a[4] = {ShopInfo::BLACKHOLE, ShopInfo::ICE, ShopInfo::FIRE, ShopInfo::STORM};
    for (int id = 0; id < 4; id++) {
        if (Cs::pi->shopItems.get(a[id]) == 1) {
            Special s;
            s.mc = interDm->attach("mcSpecial", 0);
            s.mc->gotoAndStop(id + 1);
            s.id = id;
            s.sid = a[id];
            specials.push_back(s);
        }
    }
}

void Game::placeSpecials() {
    int ssize = 12;
    for (size_t id = 0; id < specials.size(); id++) specials[id].mc->x = Cs::mcw - id * (double)ssize;
}

void Game::useSpecial(int id) {
    if (specials.empty()) return;
    Special mc;
    bool found = false;
    for (size_t i = 0; i < specials.size(); i++)
        if (specials[i].id == id) {
            mc = specials[i];
            specials.erase(specials.begin() + i);
            found = true;
            break;
        }
    if (id < 0) {
        mc = specials.front();
        specials.erase(specials.begin());
        found = true;
    }
    if (!found) return;  // Flash would crash on mc.id here
    switch (mc.id) {
        case 0: new ev::Quasar(); break;
        case 1:
            for (auto* b : std::vector<el::Ball*>(balls)) b->setType(Cs::BALL_ICE);
            break;
        case 2:
            for (auto* b : std::vector<el::Ball*>(balls)) b->setType(Cs::BALL_FIRE);
            break;
        case 3:
            for (auto* b : std::vector<el::Ball*>(balls)) b->setType(Cs::BALL_VOLT);
            break;
    }
    hasSpecialSpent = true;
    specialSpent.push_back(mc.sid);
    mc.mc->removeMovieClip();
    placeSpecials();
    incMissile(0);
    setFlash(1);
    if (flPause) {
        togglePause();
        pauseCoef = 0;
    }
}

// GRID
void Game::initLevel(int x, int y, int zid, bool flMinerai, const std::string* lvl) {
    Module::initLevel(x, y, zid, flMinerai, lvl);
    initBg();
    pad->init();
    initGrid();
    fillGrid();
}

void Game::initGrid() { grid.assign(Cs::XMAX, std::vector<Block*>(Cs::YMAX, nullptr)); }

void Game::fillGrid() {
    bdm->clear(0);
    level->genModel();
    level->genPalette();
    block = 0;
    blocks.clear();
    level->genBonusTable();
    if (Cs::pi->gotItem(MissionInfo::MINES)) {
        int max = 1;
        if (Cs::pi->shopItems.get(ShopInfo::MINE_0) == 1) max++;
        if (Cs::pi->shopItems.get(ShopInfo::MINE_1) == 1) max++;
        if (Cs::pi->shopItems.get(ShopInfo::MINE_2) == 1) max++;
        for (int i = 0; i < max; i++) level->addMine();
    }
    for (int y = 0; y < Cs::YMAX; y++)
        for (int x = 0; x < Cs::XMAX; x++) {
            hx::Cell c = level->model.get(x, y);
            if (c.empty()) continue;
            int type = (int)c.v;
            if (level->flDepleted && type >= Block::BONUS && type < Block::BONUS + Block::BONUS_MAX) type = Block::DEPLETED;
            new Block(x, y, type);
        }
    blockTotal = block;
}

void Game::hit(int px, int py, Hitter* ball) {
    if (Block* b = gridAt(px, py)) b->damage(ball);
}

void Game::killZone(int px, int py) {
    auto* a = monsters(px, py);
    if (!a) return;
    while (!a->empty()) {
        el::Molecule* m = a->back();
        a->pop_back();
        m->explode();
    }
}

// TITLES
void Game::newTitle(const std::string& str, int col, bool flBlink, double time) {
    MC mc = dm->attach("mcTitle", DP_INTER);
    MC fieldClip = mc->clip("mcField");
    if (fieldClip)
        if (auto f = fieldClip->field("field")) f->setText(str);
    mc->var("bl") = 100;
    mc->var("t") = hx::isNull(time) ? 32 : time;
    mc->y = 12;
    mc->yscale = 10;
    if (fieldClip) {
        if (!flBlink) fieldClip->stop();
        Filt::glow(fieldClip.get(), 4, 2, col);
    }
    titles.insert(titles.begin(), mc);
}

void Game::updateTitle() {
    for (size_t i = 0; i < titles.size(); i++) {
        MC mc = titles[i];
        double& t = mc->var("t");
        double& bl = mc->var("bl");
        t -= mt::Timer::tmod;
        if (i == 0 && t > 0) {
            bl *= 0.5;
            if (bl < 0.5) bl = 0;
            mc->yscale = std::max(100 - bl, 10.0);
        } else {
            mc->yscale *= 0.75;
            bl += 20;
            if (bl > 100) {
                mc->removeMovieClip();
                titles.erase(titles.begin() + i);
                i--;
                continue;
            }
        }
        if (bl > 0) {
            mc->filters.clear();
            Filt::blur(mc.get(), bl, 0);
        } else if (!mc->filters.empty()) {
            // the original only resets filters while blurred; keep the last blur(0) like Flash
        }
    }
}

// LISTENERS
void Game::mouseDown() {
    Module::mouseDown();
    autoLaunchTimer = 0;
    if (mcTitle) mcTitle->var("timer") = 0;
    if (pad) pad->action();
}

void Game::mouseUp() {
    Module::mouseUp();
    if (pad) pad->release();
}

void Game::mouseMove() {
    Module::mouseMove();
    if (pad) pad->flMouse = true;
}

// PLASMA
void Game::initPlasma() {
    mcPlasma = dm->empty(DP_PLASMA);
    plasmaBmp = std::make_shared<flash::BitmapData>(flash::Stage::get(), hx::Int(Cs::mcw * Cs::PQ),
                                                    hx::Int(Cs::mch * Cs::PQ), true, 0x00000000);
    mcPlasma->xscale = mcPlasma->yscale = 100 / Cs::PQ;
    mcPlasma->attachBitmap(plasmaBmp, 0);
    mcPlasma->blendMode = flash::Blend::Add;
}

void Game::updatePlasma() {
    if (!plasmaBmp) return;
    double bl = std::max(2.0, mt::Timer::tmod * 4 * Cs::PQ);
    plasmaBmp->applyBlur(bl, bl);
    plasmaBmp->colorTransform(flash::ColorTransform(1, 1, 1, 1, 0, 0, 0, -2));
}

void Game::plasmaDraw(flash::MovieClip* mc) {
    if (!plasmaBmp || !mc) return;
    flash::Matrix m;
    m.scale((mc->xscale / 100) * Cs::PQ, (mc->yscale / 100) * Cs::PQ);
    m.rotate(mc->rotation * 0.0174);
    m.translate(mc->x * Cs::PQ, mc->y * Cs::PQ);
    flash::ColorTransform ct(1, 1, 1, mc->alpha / 100, 0, 0, 0, 0);
    plasmaBmp->draw(mc, m, &ct, mc->blendMode);
}

void Game::displayScore(double x, double y, int sc, int col, double size) {
    auto* psc = new Phys(dm->attach("mcScore", DP_PARTS));
    psc->x = x;
    psc->y = y;
    psc->vy = -0.5;
    psc->timer = 30;
    if (auto f = psc->root->field("field")) {
        f->setText(std::to_string(sc));
        Filt::glow(f.get(), 4, 2, col);
    }
    psc->fadeLimit = 5;
    psc->fadeType = 0;
    psc->setScale(100 * size);
}

// FX
void Game::setFlash(double c, double inc, double pow) {
    if (!mcFlash) {
        mcFlash = dm->attach("mcFlash", DP_FRONT);
        mcFlash->blendMode = flash::Blend::Add;
    }
    mcFlashC = c;
    mcFlashInc = inc;
    mcFlashPow = pow;
}

void Game::updateFlash() {
    mcFlashC = Num::mm(0, mcFlashC + mcFlashInc * mt::Timer::tmod, 5);
    if (mcFlashC == 0) {
        mcFlash->removeMovieClip();
        mcFlash = nullptr;
        return;
    }
    mcFlash->alpha = std::pow(mcFlashC, mcFlashPow) * 100;
}

void Game::swapScreen() {
    flSwap = !flSwap;
    if (flSwap) {
        root->yscale = -100;
        root->y = Cs::mch;
    } else {
        root->yscale = 100;
        root->y = 0;
    }
}

// TOOLS
el::Ball* Game::newBall() { return new el::Ball(dm->attach("mcBall", DP_BALL)); }

el::Ball* Game::getLowestBall() {
    el::Ball* ball = nullptr;
    for (auto* b : balls)
        if (!ball || (b->flUp && b->y > ball->y && b->vy > 0))
            if (hx::isNull(b->gluePoint)) ball = b;
    return ball;
}

// KILL
void Game::kill() {
    if (plasmaBmp) plasmaBmp->dispose();
    if (bmpBg) bmpBg->dispose();
    auto list = Sprite::spriteList;
    for (auto* sp : list) sp->kill();
    me = nullptr;
    Module::kill();
}

// AUTO
void Game::updateAuto() {
    if (!pad) return;
    if (pad->type == Cs::PAD_LASER || pad->type == Cs::PAD_GLUE || pad->chargeTimer > 30 + hx::random(100)) {
        if (flPress) mouseUp();
        if (hx::random() < 0.07) mouseDown();
    }
}

// PAUSE
void Game::togglePause() {
    if (step != Play) return;
    Module::togglePause();
}

void Game::initKeyListener() {
    kl = std::make_shared<flash::Listener>();
    kl->onKeyDown = [this] { pressKey(); };
    flash::Stage::get()->keyListeners.push_back(kl);
}

void Game::pressKey() {
    int n = flash::Stage::get()->lastKey;
    switch (n) {
        case 13: useSpecial(); break;
        case 80: togglePause(); break;
        case 27: togglePause(); break;
    }
}
