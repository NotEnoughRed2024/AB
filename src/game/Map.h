// The galaxy map (navi.Map) and its menus (navi.Menu, navi.menu.Shop, navi.menu.Pref), ported from
// client/src/navi/*.hx. The map shows the 20x20 sectors around the player, lets them pick a sector in
// engine range, zooms into it to start the level, and offers the astroport shop and preferences.
#pragma once
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Game.h"
#include "mt/Random.h"

namespace navi {

class Map;

// navi.Menu: a window that grows out of its menu button
class Menu {
public:
    static constexpr int MARGIN = 10;
    Menu(double x, double y);
    virtual ~Menu();
    void updateMenu();
    virtual void update() {}
    bool dead = false;

protected:
    int step = 0;
    int sens = 1;
    double coef = 0;
    Map* map;
    MC root;
    MC mcDraw;
    double icx, icy;
    std::unique_ptr<mt::DepthManager> dm;

    virtual void init();
    void initSquare(double c);
    void updateSquare();
    void kill();
    virtual void quit();
};

class Shop : public Menu {
public:
    Shop(double x, double y) : Menu(x, y) {}
    static void initAlien(flash::MovieClip* mc, const mt::OldRandom* seed = nullptr);
    static const std::vector<std::vector<int>> PALETTE;

private:
    struct Slot {
        MC mc;
        int id = -1;
    };
    MC skin;
    std::unique_ptr<mt::DepthManager> sdm;
    std::vector<Slot> slots;
    int selection = -1;  // index into slots
    std::unique_ptr<mt::OldRandom> seed;

    void init() override;
    void quit() override;
    void initSeed();
    void genContent();
    void cleanContent();
    bool isVisible(int n) const;
    bool isBuyable(int n) const;
    void select(int i);
    void unselect();
    void buyItem();
    void active(const MC& mc, std::function<void()> f);
    void unactive(const MC& mc);
};

class Pref : public Menu {
public:
    Pref(double x, double y) : Menu(x, y) {}
    void update() override;

private:
    static constexpr double START = 87, RANGE = 226;
    MC skin;
    std::vector<MC> bools;
    int dragging = -1;
    void init() override;
    void toggleBool(int id);
    void setVal(int id, double c);
};

class Map {
    friend class Menu;

public:
    static constexpr int ZONE_MARGIN = 10, XMAX = 20, YMAX = 20, BW = 20, BH = 18;
    static constexpr int DP_GAME = 0, DP_MAP = 1, DP_LAYER = 2, DP_WINDOW = 3, DP_COORD = 4, DP_INTER = 5;
    static constexpr int DP_BG = 0, DP_FOG = 1, DP_MOVE = 2, DP_ICONS = 3;
    static int SX, SY;
    static Map* me;

    explicit Map(MC mc);
    ~Map();

    void init();
    void update();
    void setInfos();
    void initConnexion();
    void callPlay(int x, int y);
    void confirmMove(int x, int y, bool flMinerai);
    void warp(); // Api.warp, offline emulation

    void switchView(bool vis);
    void drawFog();
    void newMenu(int id, std::function<void()> f = nullptr);
    void removeMenu(int id);

    bool flActive = false, flView = true, flFuel = false;
    std::unique_ptr<Game> game;
    std::unique_ptr<Menu> menu;
    std::unique_ptr<mt::DepthManager> dm, mdm;
    int spaceColors[XMAX][YMAX] = {};
    int zoneTable[XMAX][YMAX] = {};  // -1 = null

    bool isPlaying() const { return step == Play; }
    bool isMoving() const { return step == Move; }

private:
    enum Step { None, Move, Connexion, Zoom, Hole, Land, Play, Error } step = None;
    int zx = 0, zy = 0, zsens = 0;  // Zoom(x, y, sens)
    int holeStep = 0;
    int hx_ = XMAX / 2, hy_ = YMAX / 2;
    double zoomCoef = 0, zoomSpeed = 0, iconBlink = 0, menuTimer = 0;

    struct Star {
        MC mc;
        double c, dx, dy, fonce;
    };
    struct HeroMove {
        MC icon;
        double c;
        int sx, sy, ex, ey;
    };
    struct MenuButton {
        MC mc;
        int id;
    };

    std::vector<std::vector<std::unique_ptr<mt::Random>>> seedTable;
    std::vector<int> zones;  // zone ids in view
    int fog[XMAX][YMAX] = {};
    bool exp[XMAX][YMAX] = {};
    std::map<std::pair<int, int>, bool> reach;  // missing = null (Haxe's reach[x][y] == null)
    bool pendingMove = false;
    int pendingX = 0, pendingY = 0;
    std::vector<MC> icons;
    std::vector<bool> iconBlinks;
    std::vector<Star> stars;
    std::vector<MenuButton> menus;
    std::unique_ptr<HeroMove> heroMove;
    std::unique_ptr<Menu> deadMenu;

    flash::BMP bmpBg, bmpFog;
    struct Layer {
        MC mc;
        flash::BMP bmp;
        std::unique_ptr<mt::DepthManager> dm;
        MC pad, mask;
    };
    std::unique_ptr<Layer> layer;

    MC root, map, bg, mcGame, mcScreenshot, mcCoord, mcFarWarning, mcMoveZone, miniPad, box;
    flash::BMP screenshotBmp;
    double moveZoneDec = 0, miniPadVy = 0, miniPadVr = 0;

    void initMove();
    void updateMove();
    void updateCoord();
    std::string getSquareName(int x, int y) const;
    void startZoom(int x, int y, bool flMinerai);
    void updateZoom();
    void initStars();
    void updateStars(double zc, double cx, double cy);
    void initHole();
    void updateHole();
    void updateLand();
    void initPlay();
    void cleanAll();
    void initZones();
    void initMenu();
    void updateMenu();
    void launchMenu(MenuButton& b);
    void active();
    void unactive();
    void initMap();
    void drawBg();
    void clickMap();
    void updateHero();
    void initIcons();
    MC displayIcon(int id, int x, int y, bool flBlink = false);
    void updateIcons();
    void removeIcons();
    void initMoveZone();
    std::vector<std::array<int, 2>> extendZone(const std::vector<std::array<int, 2>>& zone);
    struct P {
        int x, y;
    };
    P getMouseSector() const;
    bool isZoneIn(const int pos[3]) const;
    void initBoxFuel();
    void placeEarth();
    bool inGrid(int x, int y) const { return x >= 0 && y >= 0 && x < XMAX && y < YMAX; }
};

}  // namespace navi
