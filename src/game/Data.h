// Game data (zones, missions, items, shop, text) and the player's state, ported from com/*.hx.
#pragma once
#include <SDL3/SDL.h>  // before json.hpp (MinGW: <intrin.h> must precede <cpuid.h>)
#include <array>
#include <climits>
#include <string>
#include <vector>

#include "Constants.h"
#include "json.hpp"

// ------------------------------------------------------------------ static data

struct Zone {
    std::string name;
    int pos[3] = {0, 0, 0};
    int col = 0;
    std::vector<std::array<int, 6>> pal;
};

struct ItemPos {
    bool has = false;  // x/y are null in the original for items that aren't placed on the map
    int x = 0, y = 0, fam = 0;
};

struct Mission {
    std::string name, desc, end;
    std::vector<std::vector<int>> conditions, startConditions, startItem, endItem;
};

struct ShopItem {
    std::vector<int> pb;
    int price = 0;
};

namespace ZoneInfo {
extern std::vector<Zone> list;
extern std::vector<std::array<std::array<int, 2>, 2>> holes;
int getPlanet(int x, int y);  // -1 = none
bool isInCircle(int x, int y, int cx, int cy, int rad);
}  // namespace ZoneInfo

namespace MissionInfo {
extern std::vector<ItemPos> ITEMS;
extern std::vector<Mission> LIST;
inline const ItemPos& item(int id) {
    static ItemPos none;
    return (id >= 0 && id < (int)ITEMS.size()) ? ITEMS[id] : none;
}
}  // namespace MissionInfo

namespace ShopInfo {
extern std::vector<ShopItem> ITEMS;
}

namespace Text {
bool load(const std::string& dataDir, const std::string& lang);
std::string str(const std::string& key);
std::string at(const std::string& key, int i);
int count(const std::string& key);
}  // namespace Text

bool loadGameData(const std::string& dataDir);

// ------------------------------------------------------------------ player

// Haxe arrays with holes: NONE marks null/undefined entries.
struct SparseInts {
    static constexpr int NONE = INT_MIN;
    std::vector<int> v;
    int get(int i) const { return (i >= 0 && i < (int)v.size()) ? v[i] : NONE; }
    void set(int i, int val) {
        if (i < 0) return;
        if (i >= (int)v.size()) v.resize(i + 1, NONE);
        v[i] = val;
    }
    int size() const { return (int)v.size(); }
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

// Offline stand-ins for what the website recorded: when each mission started/ended (for the
// mission feed) and a few personal records (for the "Records" tab that replaces the rankings).
struct MissionLogEntry {
    int mission = 0;
    int state = 0;      // 0 = received, 1 = completed
    long long time = 0; // unix seconds
};

struct PlayerStats {
    int levels = 0, cleared = 0, lost = 0, mineralsTotal = 0, bestMinerals = 0, missilesFired = 0;
    long long playSeconds = 0, firstPlayed = 0;
};

struct Travel {
    std::string name;
    int sx = 0, sy = 0, ex = 0, ey = 0, start = 0, dest = 0;
};

class PlayerInfo {
public:
    static bool FL_RESET;
    static int DEFAULT_X, DEFAULT_Y;

    bool flAdmin = false, flEditor = false;
    int pid = 0;
    int ox = 0, oy = 0, x = 0, y = 0;
    int chl = 0, chs = 0, minerai = 0;
    SparseInts missions;
    int missile = 0, missileMax = 0;
    int life = 3;
    int engine = 1, radar = 1, drone = 0;
    int pendingLevels = 0;
    std::vector<int> fog;
    SparseInts items, shopItems;
    std::vector<int> comp;
    std::vector<Travel> travel;
    std::vector<std::vector<int>> houseDone;
    std::vector<MissionLogEntry> log;
    PlayerStats stats;
    void logMission(int n, int state);

    PlayerInfo();
    void setToDefault();
    int getLife() const { return life - (int)travel.size(); }
    int getRay() const;
    int getMissileType() const;
    int getMissileCadence() const;
    double getMissileTurnSpeed() const;
    bool gotItem(int n) const;

    void loadCache();
    void saveCache();
    void addItem(int id);  // id < 0 = none
    void buyShopItem(int id);
    int getPrice(int id) const;
    void checkMission();
    bool isAllConditionsOk(const std::vector<std::vector<int>>& list) const;
    bool isConditionOk(const std::vector<int>& a) const;
    void addStuff(const std::vector<int>& a);
};
