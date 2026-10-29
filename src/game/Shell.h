// Top level: what Manager.hx and Api.hx (the offline "server emulation") do around the galaxy map.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "Game.h"

namespace navi {
class Map;
}

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

    // called by the game and the map
    void initConnexion();
    void endGame(int x, int y, bool victory, int min, int missiles, int itemId, const std::vector<int>& shopItems);
    void reloadPlayer();   // fresh PlayerInfo from the save (part of Map.setInfos)
    void requestInfos() { pendingInfos = true; }  // haxe.Timer.delay(Api.setInfos)
    void levelStarted();
    Game* game() const;
    navi::Map* getMap() const { return map.get(); }

    bool quitRequested = false;

private:
    static Shell* instance;
    std::unique_ptr<PlayerInfo> pi;
    std::unique_ptr<navi::Map> map;
    bool pendingInfos = false;
    Uint64 levelStartMs = 0;
    int directX = 0, directY = 0;
    bool directPending = false;
};
