// Alphabounce gameplay, ported from client/src/*.hx (Motion Twin, 2007).
// The structure mirrors the original classes so the two can be read side by side.
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Data.h"
#include "mt/Bumdum.h"
#include "mt/DepthManager.h"
#include "mt/Haxe.h"
#include "mt/Random.h"
#include "mt/Timer.h"

using flash::MC;
using flash::TF;
using mt::Phys;
using mt::Sprite;

class Game;
class Level;
class Block;
class Pad;
class Option;
class Event;
namespace el {
class Ball;
class Molecule;
}  // namespace el
namespace ev {
class Drone;
}

// ------------------------------------------------------------------ Cs

struct Cs {
    static bool DEMO;
    static constexpr int BW = 28;
    static constexpr int BH = 14;
    static const int DIR[4][2];
    static int MX, MY;
    static int XMAX, YMAX;
    static double SIDE;
    static double PREF_MOUSE, PREF_GFX;
    static std::vector<bool> PREF_BOOLS;
    static constexpr int mcw = 400;
    static constexpr int mch = 360;
    static PlayerInfo* pi;
    static int seed;
    static constexpr int COL_SPACE = 0x000050;
    static constexpr int MAX_BALL = 18;
    static constexpr int MAX_OPTION = 6;

    static constexpr int BALL_STANDARD = 0, BALL_FIRE = 1, BALL_ICE = 2, BALL_DRUNK = 3, BALL_KAMIKAZE = 4,
                         BALL_YOYO = 5, BALL_HALO = 6, BALL_SHADE = 7, BALL_VOLT = 8;
    static constexpr int PAD_STANDARD = 0, PAD_GLUE = 1, PAD_TIME = 2, PAD_LASER = 3, PAD_GENERATOR = 4,
                         PAD_AIMANT = 5, PAD_SHAKE = 6;
    static constexpr int TEMPO = 120;
    static constexpr double DOOR_COEF = 0.25;
    static constexpr double OPTION_COEF = 0.1;
    static constexpr double PQ = 0.3;

    static void init();
    static void loadPref();
    static double getX(double px) { return SIDE + px * BW; }
    static double getY(double py) { return py * BH; }
    static int getPX(double x) { return hx::Int((x - SIDE) / BW); }
    static int getPY(double y) { return hx::Int(y / BH); }
    static double getPerfCoef() { return std::max(0.0, 1 - (Sprite::spriteList.size() / 120.0)); }
    static int getDir(double n) { return hx::Int(mt::Num::sMod(n, 4)); }
    static void log(const std::string& s);
};

// Something that can hit a block: a ball, a shot, or the anonymous {type:0,damage:1} of ev.Wave.
// Fields the original object lacks read as undefined in Flash; -1 / no-op models that here.
struct Hitter {
    virtual ~Hitter() = default;
    virtual int hType() const { return -1; }
    virtual int hFam() const { return -1; }
    virtual double hDamage() const { return 0; }
    virtual void hDestroy() {}
    virtual int hPx() const { return 0; }
    virtual int hPy() const { return 0; }
};

// ------------------------------------------------------------------ Element / Bouncer

class Element : public Sprite, public Hitter {
public:
    explicit Element(MC mc);
    bool flGhost = false;
    int px = 0, py = 0;
    double vx = 0, vy = 0;
    void update() override;
    virtual void setGhost(bool fl);
    void updateGhost();
    void updatePos() override;
    void moveTo(double nx, double ny);
    flash::Point getPos() const { return {Cs::getX(px) + ox, Cs::getY(py) + oy}; }
    int hPx() const override { return px; }
    int hPy() const override { return py; }

protected:
    double ox = 0.5, oy = 0.5;
    double vvx = 0, vvy = 0;
    double frict = 1;
    void updateNormal();
    virtual void onBounce(int px, int py);
    virtual void onEnterSquare(int sx, int sy) {}
};

class Bouncer {
public:
    explicit Bouncer(Phys* sprite);
    double frict = 1;
    void update();
    void setPos(double nx, double ny);
    virtual void onBounce(int px, int py) {}
    virtual ~Bouncer() = default;

private:
    Phys* sp;
    int px = 0, py = 0;
    double ox = 0, oy = 0;
};

// ------------------------------------------------------------------ Event

class Event {
public:
    Event();
    virtual ~Event() = default;
    virtual void update() {}
    virtual void kill();
    bool dead = false;
};

// ------------------------------------------------------------------ Level

class Level {
public:
    static constexpr int PB_STEEL_BAR = 0, PB_STEEL_CLOUD = 1, PB_PUSHER = 2, PB_BOOM = 3, PB_JUMPER = 4,
                         PB_STORM = 5, PB_CAGE = 6, PB_GENERATOR = 7, PB_DRAGON = 8, PB_MISSILE = 9, PB_DOOR = 10,
                         PB_KILL = 11, PB_DEATH = 12;

    Level(int x, int y, int id, bool flMinerai, const std::string* level = nullptr);
    bool flDepleted = false, flEdit = false, flLure = false;
    int wx, wy;
    int zid;  // -1 = no zone
    int itemId = -1;
    double dst, ang;
    int lvl, ymax;
    bool hasStruct = false;
    hx::Grid structGrid;
    mt::OldRandom seed;
    std::vector<int> proba;
    std::vector<std::vector<int>> bonusTable;  // -1 = none
    flash::BMP bmpPaint;
    hx::Grid model;
    std::vector<double> distances;

    void genPalette();
    void genBonusTable();
    void genModel();
    void genPrimeModel();
    void forceModel();
    void genStruct();
    bool isItemVisible() const;
    bool isModelOpen(int obj);
    int getGoal() const;
    int getMineraiTotal() const;
    bool isOut(int x, int y) const { return x < 0 || x >= Cs::XMAX || y < 0 || y >= ymax; }
    void addMine();
    std::string getString() const;
    int paint(int x, int y) const;  // bmpPaint.getPixel

private:
    void initSeed();
    void initProba();
    void genZoneModif();
    void genBonusBlock(int ymax);
    std::vector<std::array<int, 2>> getHoriLine(int sy, double turnCoef, int type = -1, int hole = -1);
    void genRandom(int type, int max, int drawType = -1, int ma = 0, int ym = -1);
    void genCloud(int type);
    void insertItem();
    void searchAndReplace(int o, int n);
    flash::Point getRandomBlock();
    flash::Point getMineCoord();
    int getPath(int x, int y, std::vector<std::vector<bool>>& grid, int depth);
};

// ------------------------------------------------------------------ Block

class Block {
public:
    static constexpr int MIN_GREEN = 1, MIN_BLUE = 5, MIN_PINK = 25;
    static constexpr int BONUS_MAX = 3, MOLECULE_MAX = 10;
    static constexpr int BONUS = 10, BALL = 13, BOOM = 14, SPACE = 15, REDUC = 16, STEEL = 17, PUSHER = 18,
                         JUMPER = 19, STORM = 20, ITEM = 21, CAGE = 22, GENERATOR = 32, LURE = 42, DRAGON_LEFT = 43,
                         DRAGON_RIGHT = 44, INSECT = 45, SWAP = 46, MISSILE = 47, DOOR = 48, DEPLETED = 49, NUT = 50,
                         KILL = 51, LIFE = 52, DEATH = 53, GLUE = 54, GUARDIAN = 55, MINE = 56;

    Block(int px, int py, int t = -1, MC mc = nullptr, bool flEdit = false);
    bool flStandard = false, flEdit = false;
    bool flIce = false, flDeath = false;
    MC root;
    int x, y;
    int molType = -1;
    int type = -1;
    double life = 0;  // NaN = null (indestructible)
    double transform = 0;
    double lastShot = hx::NUL;
    std::vector<int> color;
    double explodeTimer = 0;
    Event* event = nullptr;
    std::function<void()> update;
    double respawnTimer = 0;
    void (Block::*onDamage)(Hitter*) = nullptr;
    std::unique_ptr<mt::DepthManager> dm;
    MC blink;

    void setType(int t);
    void setStandard();
    void setColor(const std::vector<int>& col);
    void setLife(double n);
    void setSkin(const MC& mc);
    void incTransform(double n, int nt = 0);
    void damage(Hitter* ball);
    void explode();
    void genMolecule();
    void removeUpdate();
    void register_();
    void unregister();
    void setPos(int px, int py);
    void fxBlink();
    void fxGlass(int max);
    void fxSparks(int max, double cr = hx::NUL, double spMax = hx::NUL, double randSize = hx::NUL);
    void fxBolt();
    void fxTwinkle(int max);
    void fxExplode(int max);
    void fxFrout(int max);
    void fxBoom();
    void kill();

    static bool isSoft(const hx::Cell& n);
    static bool isSoft(int n) { return isSoft(hx::Cell(n)); }
    static bool isSentinelle(int n) { return n == REDUC || n == STORM || n == KILL || n == GLUE || n == LURE; }
    static bool isBonus(int n) { n -= BONUS; return n >= 0 && n < 3; }
    static bool isBasic(int n) {
        return n == STEEL || n == DOOR || n == DRAGON_LEFT || n == MISSILE || n == BOOM || n == PUSHER ||
               n < BONUS + 3 || n == INSECT || n == REDUC;
    }

private:
    void iceIt();
    void shootReduc(Hitter* ball);
    void shootGenerator(Hitter* ball);
    void shootJumper(Hitter* ball);
    void shootStorm(Hitter* ball);
    void shootSwap(Hitter* ball);
    void shootKill(Hitter* ball);
    void shootGlue(Hitter* ball);
    void updateExplode();
    void setUpdate(std::function<void()> f);
    void fadeOut();
    void fadeIn();
    void fadeOutVert();
    void fadeInVert();
    void initExplode(Phys* p);
};

// ------------------------------------------------------------------ Pad

class PadSkin : public Phys {
public:
    static constexpr int SIDE = 12;
    static constexpr int HEIGHT = 10;
    explicit PadSkin(MC mc);
    double ray = 0;
    int type = -1;
    int skinId = 0;
    virtual void setType(int n);
    void setRay(double r);
    void setReactor(bool fl);
    virtual void explode(MC bmc);

protected:
    std::unique_ptr<mt::DepthManager> dm;
    MC mcReactor;
    MC part(const char* n) const;  // skin.side0 / side1 / mid
};

class Pad : public PadSkin {
public:
    static constexpr int SIDE = 12;
    static constexpr int SPEED = 10;
    static constexpr int HEIGHT = 10;
    static constexpr int DY = 1;

    explicit Pad(MC mc);
    std::vector<ev::Drone*> drones;
    bool flIce = false, flTransform = false, flAutoMissile = false, flGuardian = false;
    double ty = hx::NUL;
    double moveFactor = 0.5;
    int freeze = 0, insect = 0, opx = -1000, glueCount = 0;
    double missileCoef = hx::NUL, missileCooldown = hx::NUL;
    bool flStop = false, flAttract = false, flMouse = false;
    double padec = hx::NUL, tpadec = 0, flh = 0, power = hx::NUL, recovery = 0, chargeTimer = hx::NUL;

    el::Ball* initStartBall();
    void init();
    void update() override;
    void newPadec();
    void action();
    void release();
    void salve();
    void setType(int n) override;
    void bounceBall(el::Ball* ball);
    void initCharge();
    void explode(MC mc) override;
    void powerUp();
    void setFlash(double n = 1);
    void queue(const std::string& link, double alpha);
    static double getPadX();

private:
    std::vector<Block*> selectedBlocks;
    Block* trg = nullptr;
    MC mcCharge;
    void updatePlay();
    void move();
    void moveHeight();
    void initPower();
    void updatePower();
    void displayPowerBar();
    Block* findTransformTrg();
    void updateCharge();
    void launchCharge();
    void unSelectBlocks();
    void launchMissile();
    bool checkMissiles();
    void checkDrone(double sx, double sy);
    void fxExplosion(double x, double y);
    void updateFlash();
};

// ------------------------------------------------------------------ Option

class Option : public Phys {
public:
    static constexpr int AIMANT = 0, BLINDAGE = 1, COLLE = 2, DIMINUTION = 3, EXTENSION = 4, FLAMME = 5, GLACE = 6,
                         HALO = 7, INDIGESTION = 8, JAVELOT = 9, KAMIKAZE = 10, LASER = 11, MULTIBALL = 12,
                         NOUVELLE_BALLE = 13, OUVRE = 14, PROVISION = 15, QUASAR = 16, RALLENTISSEMENT = 17,
                         SAUVETAGE = 18, TRANSFORMEUR = 19, ULTRA_VIOLET = 20, VOLT = 21, WHISKY = 22, XANAX = 23,
                         YOYO = 24, ZELE = 25;
    static int SUM;
    static constexpr double FALL_SPEED = 3;
    static const double PROB[26];
    static std::vector<double> prob;

    explicit Option(MC mc);
    int id = 0;
    bool flItem = false;
    int color = 0;
    static int getRandomId(mt::OldRandom& seed);
    static void genProb(const Level* level);
    void setType(int n);
    void setItem(int n);
    void update() override;
    void kill() override;
    static int getCol(int id);
    static bool isBad(int id);

private:
    std::unique_ptr<mt::DepthManager> sdm;
    void apply();
    void newRay();
};

// ------------------------------------------------------------------ BadShot

class BadShot : public Phys {
public:
    explicit BadShot(MC mc);
    double height = 0;
    Block* bl = nullptr;
    void update() override;
    void setType(int n);
    void directShot(Block* bl, double speed = 6, double prec = 0);
    void frontShot(Block* bl, double speed = 6);

private:
    int type = -1;
    double yLim;
    double impact = hx::NUL;
    double ray = 2;
    bool flUp = true;
    std::string queue_;
    void hit();
};

// ------------------------------------------------------------------ Module / Game

struct Special {
    MC mc;
    int id = 0, sid = 0;
    double cx = 0;
};

class Module {
public:
    explicit Module(MC mc);
    virtual ~Module();
    bool flEndConnect = false, flPress = false, flClick = false, flVictory = false, flView = false, flPause = false;
    int wx = 0, wy = 0;
    int pauseCount = 0;
    std::unique_ptr<Level> level;
    double victoryTimer = 0;
    int min = 0;      // VarSecure
    int missile = 0;  // VarSecure
    std::vector<Special> specials;
    std::vector<Special> caps;
    bool hasCaps = false;
    MC mcEnding;
    MC mcPause;
    std::unique_ptr<mt::DepthManager> dm;
    MC root;

    virtual void initLevel(int x, int y, int zid, bool flMinerai, const std::string* lvl = nullptr);
    virtual void initPlay();
    virtual void update();
    virtual void initEnding(bool fl);
    virtual void togglePause();
    void updatePause();
    virtual void useSpecial(int id = -1) {}
    void switchView(bool fl) { flView = fl; }
    virtual void mouseDown();
    virtual void mouseUp();
    virtual void mouseMove();
    virtual void kill();

protected:
    double pauseCoef = hx::NUL;
    double capsCoef = 0;
    double flFadeName = 0;
    std::shared_ptr<flash::Listener> ml, kl;
    void updateSprites();
    flash::BMP getBmpBg(int col);
    virtual void updateEnding();
    virtual void endGame() {}
    void initMouseListener();
};

class Game : public Module {
public:
    static bool FL_DEBUG, PLAY_AUTO;
    static constexpr int DP_BG = 0, DP_UNDERPARTS = 3, DP_BLOCK = 4, DP_PLASMA = 5, DP_PAD = 6, DP_OPTION = 7,
                         DP_BALL = 8, DP_PARTS2 = 9, DP_DRONE = 10, DP_MONSTER = 11, DP_PARTS = 12,
                         DP_FRONT_PARTS = 13, DP_FRONT = 14, DP_INTER = 15, DP_PAUSE = 16;
    static Game* me;

    Game(MC mc, int col);
    ~Game() override;

    bool flFirstBall = false, flItemFall = false, flSwap = false, flSafe = false, flItemCollected = false;
    enum Step { Play, Ending } step = Play;
    int life = 0;  // VarSecure
    int missileType = 0;
    double missileCadence = 16, missileTurnSpeed = 0.2, difficulty = 1;
    int block = 0;
    int spaceColor;
    int blockTotal = 0;
    double accTimer = 0, scroll = 0, timeCoef = hx::NUL;
    double shake = hx::NUL, levelTimer = 0, autoLaunchTimer = 0, inactiveTimer = 0, respawnTimer = hx::NUL;

    std::vector<std::vector<Block*>> grid;
    Block* gridAt(int x, int y) const {
        if (x < 0 || y < 0 || x >= (int)grid.size() || y >= (int)grid[x].size()) return nullptr;
        return grid[x][y];
    }
    void gridSet(int x, int y, Block* b) {
        if (x < 0 || y < 0 || x >= (int)grid.size()) return;
        if (y >= (int)grid[x].size()) grid[x].resize(y + 1, nullptr);
        grid[x][y] = b;
    }
    std::vector<Block*> blocks;
    std::vector<Block*> crawlers;
    std::vector<std::vector<std::vector<el::Molecule*>>> monsterGrid;
    std::vector<el::Molecule*>* monsters(int x, int y) {
        if (x < 0 || y < 0 || x >= (int)monsterGrid.size() || y >= (int)monsterGrid[x].size()) return nullptr;
        return &monsterGrid[x][y];
    }
    std::vector<el::Ball*> balls;
    std::vector<Option*> options;
    std::vector<Event*> events;
    std::vector<MC> titles;
    std::vector<el::Molecule*> molecules;
    std::vector<int> specialSpent;
    bool hasSpecialSpent = false;
    Pad* pad = nullptr;
    std::unique_ptr<mt::DepthManager> bdm;
    MC base, bg, mcSunglasses, mcPlasma, mcTitle, mcInter, mcFlash, mcCursor, mcWarning;
    flash::BMP plasmaBmp, bmpBg;
    std::unique_ptr<mt::DepthManager> interDm;
    std::vector<MC> lives;

    // owned objects (freed with the game, like the Flash GC would)
    std::vector<std::unique_ptr<Block>> ownedBlocks;
    std::vector<std::unique_ptr<Event>> ownedEvents;

    void initBg();
    void newLife(int n, bool flSpark = false);
    void incMissile(int n);
    void incMinerai(int n);
    void update() override;
    void initPlay() override;
    void removeBlock();
    void initEnding(bool flVictory) override;
    void killPad();
    void newOption(int t, double x = hx::NUL, double y = hx::NUL);
    void getOption(int id);
    void initSpecials();
    void placeSpecials();
    void useSpecial(int id = -1) override;
    void initLevel(int x, int y, int zid, bool flMinerai, const std::string* lvl = nullptr) override;
    void hit(int px, int py, Hitter* ball);
    void killZone(int px, int py);
    void newTitle(const std::string& str, int col, bool flBlink = false, double time = hx::NUL);
    void mouseDown() override;
    void mouseUp() override;
    void mouseMove() override;
    void plasmaDraw(flash::MovieClip* mc);
    void displayScore(double x, double y, int sc, int col = 0x222288, double size = 1);
    void setFlash(double c = 1, double inc = -0.1, double pow = 0.5);
    void updateFlash();
    void swapScreen();
    el::Ball* newBall();
    bool isFree(int px, int py) const { return gridAt(px, py) == nullptr && px >= 0 && px < Cs::XMAX && py >= 0; }
    el::Ball* getLowestBall();
    void kill() override;
    void togglePause() override;
    void updateAuto();  // Demo mode: the game clicks for itself
    Event* addEvent(Event* e) {
        ownedEvents.emplace_back(e);
        return e;
    }
    Block* addBlock(Block* b) {
        ownedBlocks.emplace_back(b);
        return b;
    }

protected:
    void updateEnding() override;
    void endGame() override;

private:
    void drawPlanet(int zid, flash::MovieClip* mc);
    void initInter();
    void updateInter();
    void initCursor();
    void updateCursor();
    void updatePlay();
    void cleanAll();
    void newPad();
    void initGrid();
    void fillGrid();
    void updateTitle();
    void initPlasma();
    void updatePlasma();
    void initKeyListener();
    void pressKey();
    double mcFlashC = 0, mcFlashInc = 0, mcFlashPow = 0.5;
};

// Factory helpers: the original sprinkles "new X(mc)" everywhere; sprites register themselves.
template <class T, class... A>
T* make(A&&... a) {
    return new T(std::forward<A>(a)...);
}
