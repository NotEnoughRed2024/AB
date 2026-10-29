#include "Data.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <ctime>

#include "flash/Stage.h"
#include "mt/Haxe.h"
#include "mt/Random.h"

using json = nlohmann::json;

std::vector<Zone> ZoneInfo::list;
std::vector<std::array<std::array<int, 2>, 2>> ZoneInfo::holes;
std::vector<ItemPos> MissionInfo::ITEMS;
std::vector<Mission> MissionInfo::LIST;
std::vector<ShopItem> ShopInfo::ITEMS;

static json loadJson(const std::string& path) {
    size_t sz = 0;
    void* raw = SDL_LoadFile(path.c_str(), &sz);
    if (!raw) {
        SDL_Log("cannot read %s: %s", path.c_str(), SDL_GetError());
        return json();
    }
    json j = json::parse((const char*)raw, (const char*)raw + sz, nullptr, false);
    SDL_free(raw);
    return j;
}

static std::vector<std::vector<int>> intLists(const json& j) {
    std::vector<std::vector<int>> out;
    if (!j.is_array()) return out;
    for (const auto& a : j) {
        std::vector<int> v;
        for (const auto& n : a) v.push_back(n.is_number() ? (int)n : 0);
        out.push_back(v);
    }
    return out;
}

bool loadGameData(const std::string& dir) {
    json d = loadJson(dir + "/game_data.json");
    if (d.is_discarded() || d.is_null()) return false;
    for (const auto& z : d["zones"]) {
        Zone zn;
        zn.name = z["name"];
        for (int i = 0; i < 3; i++) zn.pos[i] = z["pos"][i];
        zn.col = z["col"];
        for (const auto& p : z["pal"]) {
            std::array<int, 6> a{};
            for (int i = 0; i < 6; i++) a[i] = p[i];
            zn.pal.push_back(a);
        }
        ZoneInfo::list.push_back(zn);
    }
    for (const auto& h : d["holes"]) ZoneInfo::holes.push_back({{{h[0][0], h[0][1]}, {h[1][0], h[1][1]}}});
    for (const auto& it : d["items"]) {
        ItemPos p;
        if (it.is_object() && it["x"].is_number() && it["y"].is_number()) {
            p.has = true;
            p.x = it["x"];
            p.y = it["y"];
        }
        if (it.is_object() && it["fam"].is_number()) p.fam = it["fam"];
        MissionInfo::ITEMS.push_back(p);
    }
    for (const auto& m : d["missions"]) {
        Mission mi;
        auto s = [](const json& v) { return v.is_string() ? v.get<std::string>() : std::string(); };
        mi.name = s(m["name"]);
        mi.desc = s(m["desc"]);
        mi.end = s(m["end"]);
        mi.conditions = intLists(m["conditions"]);
        mi.startConditions = intLists(m["startConditions"]);
        mi.startItem = intLists(m["startItem"]);
        mi.endItem = intLists(m["endItem"]);
        MissionInfo::LIST.push_back(mi);
    }
    for (const auto& s : d["shop"]) {
        ShopItem si;
        for (const auto& n : s["pb"]) si.pb.push_back(n);
        si.price = s["price"];
        ShopInfo::ITEMS.push_back(si);
    }
    SDL_Log("game data: %zu zones, %zu missions, %zu items", ZoneInfo::list.size(), MissionInfo::LIST.size(),
            MissionInfo::ITEMS.size());
    return true;
}

bool ZoneInfo::isInCircle(int x, int y, int cx, int cy, int rad) {
    // ZoneInfo.isInCircle (radius test with the original tolerance)
    double dx = x - cx + 0.5, dy = y - cy + 0.5;
    return std::sqrt(dx * dx + dy * dy) <= rad + 0.3;
}

int ZoneInfo::getPlanet(int x, int y) {
    for (int id = 0; id < (int)list.size(); id++) {
        const Zone& p = list[id];
        int xmin = p.pos[0] - p.pos[2], xmax = p.pos[0] + p.pos[2] - 1;
        int ymin = p.pos[1] - p.pos[2], ymax = p.pos[1] + p.pos[2] - 1;
        if (x >= xmin && x <= xmax && y >= ymin && y <= ymax) {
            if (isInCircle(x, y, p.pos[0], p.pos[1], p.pos[2])) return id;
            return -1;
        }
    }
    return -1;
}

// ------------------------------------------------------------------ Text

static json gText;

bool Text::load(const std::string& dir, const std::string& lang) {
    gText = loadJson(dir + "/text_" + lang + ".json");
    if (gText.is_null() || gText.is_discarded()) gText = loadJson(dir + "/text_en.json");
    // Text.setLang adds the missile names to ITEM_NAMES
    if (gText.contains("ITEM_NAMES")) {
        auto& names = gText["ITEM_NAMES"];
        for (int i = 0; i < MissionInfo::MISSILE_MAX - 1; i++) {
            int id = MissionInfo::MISSILE + (i + 1);
            if (id <= (int)names.size()) names.insert(names.begin() + id, "Missile " + std::to_string(i + 2));
        }
    }
    return !gText.is_null();
}

std::string Text::str(const std::string& key) {
    auto it = gText.find(key);
    if (it == gText.end() || !it->is_string()) return key;
    return *it;
}

std::string Text::at(const std::string& key, int i) {
    auto it = gText.find(key);
    if (it == gText.end() || !it->is_array() || i < 0 || i >= (int)it->size()) return "";
    const json& v = (*it)[i];
    return v.is_string() ? v.get<std::string>() : std::string();
}

int Text::count(const std::string& key) {
    auto it = gText.find(key);
    return (it == gText.end() || !it->is_array()) ? 0 : (int)it->size();
}

// ------------------------------------------------------------------ PlayerInfo

bool PlayerInfo::FL_RESET = false;
int PlayerInfo::DEFAULT_X = 0;
int PlayerInfo::DEFAULT_Y = 0;

json SparseInts::toJson() const {
    json a = json::array();
    for (int n : v) a.push_back(n == NONE ? json(nullptr) : json(n));
    return a;
}

void SparseInts::fromJson(const json& j) {
    v.clear();
    if (!j.is_array()) return;
    for (const auto& n : j) v.push_back(n.is_number() ? (int)n : NONE);
}

PlayerInfo::PlayerInfo() {
    fog.assign(20 * 20, 1);  // DEFOG
}

void PlayerInfo::setToDefault() {
    flAdmin = false;
    flEditor = false;
    pid = hx::random(200);
    x = y = DEFAULT_X;
    y = DEFAULT_Y;
    ox = DEFAULT_X;
    oy = DEFAULT_Y;
    chl = 2;
    chs = 0;
    minerai = 0;
    missions = SparseInts();
    missions.set(0, 0);
    log.clear();
    logMission(0, 0);
    stats = PlayerStats();
    stats.firstPlayed = (long long)time(nullptr);
    missile = 0;
    missileMax = 0;
    engine = 1;
    radar = 1;
    life = 3;
    drone = 0;
    pendingLevels = 0;
    items = SparseInts();
    items.set(0, MissionInfo::TRIGGER);
    shopItems = SparseInts();
    comp.assign(18, 0);
    travel.clear();
    houseDone.clear();
    square.clear();
    sectors.clear();
}

void PlayerInfo::buildFog(int sx, int sy) {
    fog.assign(20 * 20, -1);
    int i = 0;
    for (int x = 0; x < 20; x++)
        for (int y = 0; y < 20; y++) fog[i++] = sectorState(sx + x, sy + y);
}

int PlayerInfo::clearedCount() const {
    int n = 0;
    for (auto& [k, v] : sectors)
        if (v == 1) n++;
    return n;
}

int PlayerInfo::getRay() const {
    int sr = 32;
    if (gotItem(MissionInfo::EXTENSION)) sr += 10;
    if (gotItem(MissionInfo::EXTENSION_2)) sr += 8;
    if (gotItem(MissionInfo::EXTENSION_3)) sr += 10;
    if (gotItem(MissionInfo::EVASION)) sr -= 10;
    return sr;
}

int PlayerInfo::getMissileType() const {
    int n = 0;
    if (gotItem(MissionInfo::MISSILE_BLUE)) n = 1;
    if (gotItem(MissionInfo::MISSILE_BLACK)) n = 2;
    if (gotItem(MissionInfo::MISSILE_RED)) n = 3;
    return n;
}

int PlayerInfo::getMissileCadence() const { return shopItems.get(ShopInfo::COOLER) == 1 ? 8 : 16; }
double PlayerInfo::getMissileTurnSpeed() const { return shopItems.get(ShopInfo::LATERAL) == 1 ? 0.35 : 0.2; }

bool PlayerInfo::gotItem(int n) const {
    int v = items.get(n);
    return v == MissionInfo::COLLECTED || v == MissionInfo::COLLECTED_INV;
}

void PlayerInfo::loadCache() {
    json& so = flash::Stage::get()->sharedObject("info");
    if (!so.contains("x") || so["x"].is_null() || FL_RESET || !so.contains("missions") || so["missions"].empty()) {
        setToDefault();
        saveCache();
        return;
    }
    flAdmin = false;  // the original test build turned this on; the offline port keeps it off
    flEditor = false;
    // the offline build reset pid to 0 and ox/oy to the current position; the website kept them, which
    // places Earth consistently and lets the map animate the last move, so the port keeps them too
    pid = so.value("pid", 0);
    x = so.value("x", 0);
    y = so.value("y", 0);
    ox = so.contains("ox") && so["ox"].is_number() ? (int)so["ox"] : x;
    oy = so.contains("oy") && so["oy"].is_number() ? (int)so["oy"] : y;
    // `square` is the area of the server's daily "zone cleanup" mission (the ES frame on the map). The
    // offline build saved a fixed test value [0,5,3] whatever the missions were; that one is dropped.
    square.clear();
    if (so["square"].is_array())
        for (auto& n : so["square"]) square.push_back(n.is_number() ? (int)n : 0);
    if (square == std::vector<int>{0, 5, 3} && !so.value("squareReal", false)) square.clear();
    chl = so.value("chl", 0);
    chs = so.value("chs", 0);
    minerai = so.value("minerai", 0);
    missions.fromJson(so["missions"]);
    missile = so.value("missile", 0);
    missileMax = so.value("missileMax", 0);
    engine = so.value("engine", 1);
    radar = so.value("radar", 1);
    life = so.value("life", 3);
    drone = so.value("drone", 0);
    items.fromJson(so["items"]);
    shopItems.fromJson(so["shopItems"]);
    comp.clear();
    if (so["comp"].is_array())
        for (auto& n : so["comp"]) comp.push_back(n.is_number() ? (int)n : 0);
    travel.clear();
    if (so["travel"].is_array())
        for (auto& t : so["travel"]) {
            Travel tr;
            tr.name = t.value("name", "");
            tr.sx = t.value("sx", 0);
            tr.sy = t.value("sy", 0);
            tr.ex = t.value("ex", 0);
            tr.ey = t.value("ey", 0);
            tr.start = t.value("start", 0);
            tr.dest = t.value("dest", 0);
            travel.push_back(tr);
        }
    houseDone.clear();
    if (so["houseDone"].is_array())
        for (auto& h : so["houseDone"]) {
            std::vector<int> v;
            for (auto& n : h) v.push_back(n.is_number() ? (int)n : 0);
            houseDone.push_back(v);
        }
    items.set(MissionInfo::MINES, SparseInts::NONE);  // the original clears this on every load

    log.clear();
    if (so["log"].is_array()) {
        for (auto& e : so["log"])
            if (e.is_array() && e.size() >= 3) log.push_back({(int)e[0], (int)e[1], (long long)e[2]});
    } else {
        // saves from before the mission feed existed: rebuild the log without dates
        for (int n = 0; n < missions.size(); n++) {
            int st = missions.get(n);
            if (st == SparseInts::NONE) continue;
            log.push_back({n, 0, 0});
            if (st == 1) log.push_back({n, 1, 0});
        }
    }
    sectors.clear();
    if (so["sectors"].is_array()) {
        for (auto& e : so["sectors"])
            if (e.is_array() && e.size() >= 3) sectors[{(int)e[0], (int)e[1]}] = (int)e[2];
    } else {
        // saves from before the map remembered sectors: the envelope only ever stands on cleared ones
        sectors[{x, y}] = 1;
    }
    const json& js = so["stats"];
    if (js.is_object()) {
        stats.levels = js.value("levels", 0);
        stats.cleared = js.value("cleared", 0);
        stats.lost = js.value("lost", 0);
        stats.mineralsTotal = js.value("mineralsTotal", 0);
        stats.bestMinerals = js.value("bestMinerals", 0);
        stats.missilesFired = js.value("missilesFired", 0);
        stats.playSeconds = js.value("playSeconds", 0LL);
        stats.firstPlayed = js.value("firstPlayed", 0LL);
    }
}

void PlayerInfo::logMission(int n, int state) {
    log.push_back({n, state, (long long)time(nullptr)});
}

void PlayerInfo::saveCache() {
    auto* st = flash::Stage::get();
    json& so = st->sharedObject("info");
    so["flAdmin"] = flAdmin;
    so["flEditor"] = flEditor;
    so["pid"] = pid;
    so["x"] = x;
    so["y"] = y;
    so["ox"] = ox;
    so["oy"] = oy;
    so["chl"] = chl;
    so["chs"] = chs;
    so["minerai"] = minerai;
    so["missions"] = missions.toJson();
    so["missile"] = missile;
    so["missileMax"] = missileMax;
    so["engine"] = engine;
    so["radar"] = radar;
    so["life"] = life;
    so["drone"] = drone;
    if (square.size() >= 3) {
        so["square"] = square;
        so["squareReal"] = true;
    } else {
        so["square"] = nullptr;
    }
    so["items"] = items.toJson();
    so["shopItems"] = shopItems.toJson();
    so["comp"] = comp;
    json tr = json::array();
    for (auto& t : travel)
        tr.push_back({{"name", t.name}, {"sx", t.sx}, {"sy", t.sy}, {"ex", t.ex}, {"ey", t.ey}, {"start", t.start}, {"dest", t.dest}});
    so["travel"] = tr;
    so["houseDone"] = houseDone;
    json sc = json::array();
    for (auto& [k, v] : sectors) sc.push_back({k.first, k.second, v});
    so["sectors"] = sc;
    json lg = json::array();
    for (auto& e : log) lg.push_back({e.mission, e.state, e.time});
    so["log"] = lg;
    so["stats"] = {{"levels", stats.levels}, {"cleared", stats.cleared}, {"lost", stats.lost},
                   {"mineralsTotal", stats.mineralsTotal}, {"bestMinerals", stats.bestMinerals},
                   {"missilesFired", stats.missilesFired}, {"playSeconds", stats.playSeconds},
                   {"firstPlayed", stats.firstPlayed}};
    st->flushSharedObject("info");
}

void PlayerInfo::addItem(int id) {
    if (id < 0) return;
    int n = items.get(id);
    items.set(id, n == MissionInfo::TRIGGER ? MissionInfo::COLLECTED_INV : MissionInfo::COLLECTED);
    if (id >= MissionInfo::MISSILE && id < MissionInfo::MISSILE + MissionInfo::MISSILE_MAX) {
        missileMax++;
        missile = missileMax;
    }
    if (id == MissionInfo::SUPER_RADAR) radar++;
}

void PlayerInfo::buyShopItem(int id) {
    switch (id) {
        case ShopInfo::CHS: chs++; break;
        case ShopInfo::AMMO: missile = missileMax; break;
        default:
            shopItems.set(id, 1);
            if (id == ShopInfo::LIFE) life++;
            if (id == ShopInfo::DRONE) drone++;
            if (id == ShopInfo::RADAR) {
                radar++;
                addItem(MissionInfo::RADAR_OK);
            }
            if (id >= ShopInfo::ENGINE && id < ShopInfo::ENGINE + ShopInfo::ENGINE_MAX) {
                int pw = 2 + id - ShopInfo::ENGINE;
                if (pw > engine) engine = pw;
            }
            if (id >= ShopInfo::MISSILE && id < ShopInfo::MISSILE + 3) {
                missileMax++;
                missile = missileMax;
            }
    }
    minerai -= getPrice(id);
}

int PlayerInfo::getPrice(int id) const {
    int p;
    if (id == ShopInfo::AMMO) p = missileMax - missile;
    else if (id == ShopInfo::DRONE) p = hx::Int(std::pow(drone + 6, 4) / 100) * 100;
    else p = (id >= 0 && id < (int)ShopInfo::ITEMS.size()) ? ShopInfo::ITEMS[id].price : 0;
    mt::OldRandom seed(hx::Int(std::fabs((double)x) * 10000) + hx::Int(std::fabs((double)y) + id * 20));
    int sens = seed.random(2) * 2 - 1;
    double c = std::pow(seed.rand(), 4);
    p += hx::Int(p * 0.5 * c * sens);
    return p;
}

void PlayerInfo::checkMission() {
    for (int n = 0; n < (int)MissionInfo::LIST.size(); n++) {
        const Mission& mi = MissionInfo::LIST[n];
        if (missions.get(n) == 0 && isAllConditionsOk(mi.conditions)) {
            missions.set(n, 1);
            logMission(n, 1);
            for (auto& a : mi.endItem) addStuff(a);
        }
    }
    for (int n = 0; n < (int)MissionInfo::LIST.size(); n++) {
        const Mission& mi = MissionInfo::LIST[n];
        if (missions.get(n) == SparseInts::NONE && isAllConditionsOk(mi.startConditions)) {
            missions.set(n, 0);
            logMission(n, 0);
            for (auto& a : mi.startItem) addStuff(a);
        }
    }
}

bool PlayerInfo::isAllConditionsOk(const std::vector<std::vector<int>>& list) const {
    for (auto& a : list)
        if (!isConditionOk(a)) return false;
    return true;
}

bool PlayerInfo::isConditionOk(const std::vector<int>& a) const {
    if (a.empty()) return true;
    auto arg = [&](size_t i) { return i < a.size() ? a[i] : 0; };
    switch (a[0]) {
        case MissionInfo::GOT_ITEM:
            for (size_t n = 1; n < a.size(); n++)
                if (!gotItem(a[n])) return false;
            break;
        case MissionInfo::NOT_ITEM:
            return !gotItem(arg(1));
        case MissionInfo::GOT_SHOPITEM:
            for (size_t n = 1; n < a.size(); n++)
                if (shopItems.get(a[n]) != 1) return false;
            break;
        case MissionInfo::GOT_PLANET: {
            int i = arg(1);
            return i >= 0 && i < (int)comp.size() && comp[i] >= 100;
        }
        case MissionInfo::GOT_MINERAI:
            return arg(1) <= minerai;
        case MissionInfo::ENTER_ZONE: {
            double dx = arg(1) - x, dy = arg(2) - y;
            return std::sqrt(dx * dx + dy * dy) <= arg(3);
        }
        case MissionInfo::LEAVE_ZONE: {
            double dx = arg(1) - x, dy = arg(2) - y;
            return std::sqrt(dx * dx + dy * dy) > arg(3);
        }
        case MissionInfo::GOT_MISSION:
            return missions.get(arg(1)) == 1;
        case MissionInfo::IS_LEVEL:
            return false;
    }
    return true;
}

void PlayerInfo::addStuff(const std::vector<int>& a) {
    if (a.empty()) return;
    switch (a[0]) {
        case MissionInfo::MINERAI: minerai += a.size() > 1 ? a[1] : 0; break;
        case MissionInfo::NEW_LIFE: life++; break;
        case MissionInfo::LOSS_LIFE: life--; break;
        case MissionInfo::NEW_RADAR: radar++; break;
        case MissionInfo::LOSS_RADAR: radar--; break;
        case MissionInfo::RESET_MISSION: missions.set(a.size() > 1 ? a[1] : 0, SparseInts::NONE); break;
        default:
            // other negative codes (CHS, ENABLE_*_MISSIONS...) were handled by the server only;
            // in the original emulation they become items[-5] etc., which have no effect.
            if (a[0] >= 0) items.set(a[0], a.size() > 1 ? a[1] : SparseInts::NONE);
    }
}
