// Planet landing: lander.Game and friends (client/src/lander/*.hx) plus the menus the planets' inhabitants
// open (navi/menu/People, Furi, Traveler, ItemGiver, Info). The lander flies the pad over a 1600x700 world
// made of bitmaps: the ground, the decor drawn over it and a collision map grown from both. Flash read those
// bitmaps pixel by pixel every frame; the port keeps CPU copies for that (premultiplied ARGB, one pixel per
// stage pixel) and uploads them once for display.
#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Game.h"
#include "Map.h"

namespace lander {

class Game;
class Pad;
class Hero;
class House;
class Mineral;

// a bitmap the game reads back: premultiplied ARGB at 1 pixel per stage pixel
struct CpuBmp {
    int w = 0, h = 0;
    std::vector<uint32_t> px;
    CpuBmp() = default;
    CpuBmp(int w_, int h_) : w(w_), h(h_), px((size_t)std::max(0, w_) * std::max(0, h_), 0) {}
    uint32_t get(int x, int y) const {
        if (x < 0 || y < 0 || x >= w || y >= h) return 0;
        return px[(size_t)y * w + x];
    }
};

struct Planete {
    int type;
    double pop;  // NaN = null (uninhabited)
    double g;
    int hor, gMax, dMax;
    double hc;
    int min[2];
    std::vector<std::array<int, 3>> gri;  // ground glows: blur, strength, colour
};

enum class Scenario { None, Casino, Traveler, Seller, Swapper, Empty, Locked, Treasure, Furi, Info, ItemGiver };

// lander.Pix: a point that walks along the ground pixel by pixel
class Pix {
public:
    Pix(int x, int y, int gid, MC mc = nullptr) : x(x), y(y), gid(gid), root(std::move(mc)) {}
    virtual ~Pix() = default;
    int x, y, gid;
    MC root;
    virtual void updatePos();
    virtual void kill();
    static void movePoint(Pix* p, int sens, int depth = 0);
    double getNormal(Pix* p = nullptr, int range = 3);
    virtual bool isFree(int x, int y);
};

// lander.pix.Phys: Pix with velocity, moving one pixel at a time through the bitmap
class PixPhys : public Pix {
public:
    explicit PixPhys(MC mc);
    bool flAngleBounce = false;
    double vx = 0, vy = 0, vvx = 0, vvy = 0;
    double weight = hx::NUL;
    double colFrict = 1;
    void fly();
    void updatePos() override;

protected:
    double ox = 0.5, oy = 0.5;
    double parc = 0;
    double frict = hx::NUL;
    virtual void onBounce(int sx, int sy);
    int getDir(int x, int y) const;
};

class Mineral {
public:
    explicit Mineral(mt::OldRandom& seed);
    bool flDeath = false;
    MC root;
    int val = 0;  // VarSecure
    void setValue(int n);
    void dropToSurface();
    void consume(double n);
    void collect();
    void kill();
    flash::Point getPosModifier();

private:
    double life = 100;
};

class Drone : public mt::Phys {
public:
    Drone();
    void update() override;
    void initModeSeeker();
    bool hasMinVal = false;
    int minVal = 0;

private:
    double c = 0, frame = 0;
    Mineral* min = nullptr;
    int jumpSens = 0;  // Jump(sens); 0 = Consume
    Pad* pad;
    flash::Point dec;
    double tx = 0, ty = 0;
    void getMinPos();
    void jumpToMineral();
    void updateJump(int sens);
    void updateConsume();
};

class Pad : public PadSkin {
public:
    static constexpr int HR = 8, MARGIN = 12;
    static constexpr double WEIGHT = 0.05;
    Pad();
    void update() override;
    void explode(MC dm) override;
    void kill() override;
    void receiveDrone(Drone* d);
    void receiveHero();
    std::vector<Mineral*> minList;

private:
    struct Pod {
        MC mc;
        mt::Phys* p;
        int sens;
    };
    enum Step { Fly, Land } step = Fly;
    int controlType = 0, drones = 0;
    double angle = -1.57, acc = 0.15, fuel = 100;
    double droneTimer = hx::NUL, landTimer = hx::NUL;
    double landAngleMax = 0.5, landPodLength = 0, droneActionRay = 100, fuelConsume = 0.3;
    std::vector<Pod> pods;
    void checkCols();
    void takeOff();
    void updateFly();
    void control();
    void thrust();
    void incFuel(double inc);
    void checkLanding();
    void initLanding();
    void initLandingControl();
    void updateLanding();
    void checkTakeOff();
    void dropHero();
    double getPodPos(double px, double py);  // NaN = none
    void removePods();
    void updatePods();
    void showMinerai(int n);
    double getMouseAngle() const;
};

class Hero : public PixPhys {
public:
    static constexpr double WEIGHT = 0.5, WALK_SPEED = 2;
    explicit Hero(int skin);
    bool flWalk = false, flJump = false, flLandingReady = false, flControl = true;
    int skinId = 0;
    double run = 0, frame = 0, acc = 0.7, fuel = 0;
    int min = 0;  // VarSecure
    enum Step { Fly, Land } step = Fly;
    House* currentHouse = nullptr;
    void update();
    void updateFly();
    bool checkBalance();
    void setIcon(const std::string& label);
    void kill() override;
    bool isFree(int x, int y) override;

protected:
    void onBounce(int sx, int sy) override;

private:
    mt::Phys* icon = nullptr;
    void playAnim(const std::string& label);
    void initFly();
    void recalY();
    void initLanding();
    void updateLanding();
    void updateWalk();
    void walk(int sens);
    void checkJump();
    void updateIcon();
    void updateHouse();
    void checkMinCol();
    double getMouseAngle() const;
};

class House {
public:
    House(int rnd, Scenario scn, int itemId = -1);
    int sx = 1, x = 0, y = 0, type = 0;
    double alienBehaviour = hx::NUL;
    Scenario scn;
    int itemId = -1;  // ItemGiver(id)
    mt::OldRandom seed, eventSeed;
    MC mask, mcAlien;
    void dig();
    void update();
    void active();
    void unactive();
    void mark();
    void initAlien();
    void launchScreen();
    static Scenario getScenario(int zid, mt::OldRandom& seed, int& itemId);

private:
    Game* game;
};

class Game : public ::Module {
public:
    static constexpr int WIDTH = 1600, HEIGHT = 700, GMIN = 60;
    static constexpr int DP_BG = 0, DP_PLAN = 1, DP_INTER = 2;
    static constexpr int DP_UNDERPARTS = 0, DP_PAD = 1, DP_GROUND = 2, DP_HERO = 3, DP_FOREGROUND = 4, DP_DRONES = 5,
                         DP_PARTS = 6;
    static constexpr int DP_DECOR = 1, DP_MINERALS = 2, DP_FRONT = 4;
    static Game* me;
    static const std::vector<Planete> PLANETES;
    static bool isAlive(int id);
    // landing gear the Debug tab can lend you: the lander menu everywhere, pods, the suit
    static bool debugKit();

    Game(MC mc, int col, bool flHouseVisited);
    ~Game() override;

    bool flMarkHouse = false, flHouseVisited = false;
    int col;
    double fadeCoef = 0;
    int chs = 0, item = -1, debit = 0;  // VarSecure
    std::optional<Travel> travel;
    std::function<flash::Point()> focus;
    std::vector<House*> houses;
    std::vector<Mineral*> minerals;
    const Planete* pl = nullptr;
    enum Step { Play, End, Outro } step = Play;
    Pad* pad = nullptr;
    Hero* hero = nullptr;
    mt::OldRandom seed;
    MC mcInter, mcInterFuel, base, mcGround, mcForeground;
    CpuBmp bmpGround, bmpForeground, bmpDecor, bmpCol;
    std::unique_ptr<mt::DepthManager> bdm, gdm, fdm;

    void initLevel(int x, int y, int zid, bool flMinerai, const std::string* lvl = nullptr) override;
    void initPlay() override;
    void update() override;
    void initEnding(bool fl) override;
    void kill() override;
    void initPad();
    void initMinerals();
    Hero* newHero();
    bool isFree(double x, double y, const CpuBmp* bmp = nullptr, std::optional<uint32_t> col = std::nullopt) const;
    bool isLandingFree(double x, double y, std::optional<uint32_t> col = std::nullopt) const;
    int getGround(double x) const;
    void incMinerai(int n);
    void incCaps(int n);
    void initOutro();
    void choice(int n);
    // the CPU bitmaps get Flash's BitmapData.draw through these
    void drawClip(CpuBmp& dst, flash::DisplayObject* o, const flash::Matrix& m, flash::Blend blend = flash::Blend::Normal);
    void drawBmp(CpuBmp& dst, const CpuBmp& src, int dx, int dy);
    void addHero(Hero* h);
    void retireHero(Hero* h);
    void togglePause() override;

protected:
    void updateEnding() override;
    void endGame() override;

private:
    struct Plan {
        MC mc;
        double c = 0, bx = 0, dy = 0;
        bool flHor = false;
    };
    std::vector<Plan> plans;
    struct Air {
        MC mc;
        double c;
    };
    MC mcAir;
    std::vector<Air> airList;
    MC bg, mcScreen, table;
    flash::BMP bmpScreen, bmpBg;
    std::vector<flash::BMP> shown;  // display copies of the CPU bitmaps
    std::vector<std::unique_ptr<Hero>> heroes;
    std::vector<std::unique_ptr<House>> ownedHouses;
    std::vector<std::unique_ptr<Mineral>> ownedMinerals;
    friend class House;
    friend class Mineral;

    void initPlans();
    void updatePlans();
    void drawGround();
    void drawDecor();
    void genColBmp();
    void updateOutro();
    void initInter();
    void initKeyListener();
    void pressKey();
    flash::BMP show(const CpuBmp& b);
};

}  // namespace lander

namespace navi {

// navi.menu.People: an inhabitant talking, with answer buttons
class People : public Menu {
public:
    People(double x, double y, const mt::OldRandom* seed) : Menu(x, y, seed) {}
    void update() override;

protected:
    MC skin, mcButs;
    std::unique_ptr<mt::DepthManager> butDm;
    std::vector<MC> buts;
    double endTimer = hx::NUL;
    std::string txt;
    int cursor = -1;
    void init() override;
    void setText(const std::string& s);
    void addBut(int id, std::function<void()> f);
    void updateButPos();
    void removeAllButs();
};

class Furi : public People {
public:
    using People::People;

private:
    int rewardMin = -1, rewardCaps = -1;
    mt::OldRandom seed;
    void init() override;
    void betray();
    void accept();
};

class Info : public People {
public:
    using People::People;

private:
    mt::OldRandom seed;
    void init() override;
    void initText();
};

class Traveler : public People {
public:
    using People::People;

private:
    Travel mission;
    int rewardMin = -1, rewardCaps = -1;
    void init() override;
    void accept();
    std::string getIntroText();
};

class ItemGiver : public People {
public:
    ItemGiver(double x, double y, const mt::OldRandom* seed, int id) : People(x, y, seed), itemId(id) {}
    static lander::Scenario getScn(int n);

private:
    int itemId;
    int price = -1;
    void init() override;
    void accept();
};

}  // namespace navi
