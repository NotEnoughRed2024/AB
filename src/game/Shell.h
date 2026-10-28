// Top level: what Manager.hx, Api.hx (offline "server emulation") and navi.Map.hx do around a level.
// navi.Map isn't ported yet; a small sector-select screen stands in for it.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "Game.h"

// The background / zone computation of navi.Map.drawBg + initZones, so each sector gets the same
// space colour and zone as in the original.
struct SectorMap {
    static constexpr int XMAX = 20, YMAX = 20, BW = 20, BH = 18, ZONE_MARGIN = 10;
    int SX = 0, SY = 0;
    int spaceColors[XMAX][YMAX] = {};
    int zoneTable[XMAX][YMAX] = {};  // -1 = no zone
    void build(int sx, int sy);
};

class Shell {
public:
    static Shell* get() { return instance; }
    static std::unique_ptr<mt::DepthManager> dm;  // Manager.dm
    static MC mcPlanet;
    static void hideMouse();
    static void showMouse();

    Shell();
    ~Shell();
    bool init(const std::string& assetsDir, const std::string& dataDir, const std::string& lang);
    void update();  // Manager's onEnterFrame
    void playDirect(int wx, int wy);  // testing: jump straight into a sector

    // called by the game
    void initConnexion();
    void endGame(int x, int y, bool victory, int min, int missiles, int itemId, const std::vector<int>& shopItems);

    bool quitRequested = false;

private:
    static Shell* instance;
    std::unique_ptr<PlayerInfo> pi;
    std::unique_ptr<Game> game;
    MC mcGame;
    bool pendingInfos = false;
    Uint64 levelStartMs = 0;
    SectorMap smap;

    // sector select stand-in
    MC ui;
    TF uiText;
    int selX = 0, selY = 0;
    std::shared_ptr<flash::Listener> uiKeys;
    void setInfos();
    void openSelect();
    void closeSelect();
    void drawSelect();
    void startLevel(int wx, int wy);
};
