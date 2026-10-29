// The "My Page" inventory widget of alphabounce.com (web/swf/inv.swf). Its Haxe source isn't in the
// archive, so this is rebuilt from the compiled swf (decompiled with tools/avm1dec.py): same art, same
// layout, same rules for what is shown and the same hint texts.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "flash/Display.h"
#include "json.hpp"

namespace mt {
class DepthManager;
}

namespace site {

class Inventory {
public:
    static constexpr double W = 460, H = 320;
    Inventory();
    ~Inventory();
    bool init(const std::string& assetsDir, const std::string& dataDir, const std::string& lang);
    void build();                                 // (re)create the widget from Cs::pi
    void render(double x, double y, double scale);  // window px; scale = window px per widget px
    void pointer(double ix, double iy, bool inside);  // pointer in widget coordinates
    void update();                                // hint fade/follow (once per frame)
    bool ready() const { return lib != nullptr; }

private:
    flash::Library* lib = nullptr;
    flash::Library* planetLib = nullptr;
    nlohmann::json txt;
    flash::MC root;
    std::unique_ptr<mt::DepthManager> dm;
    flash::BMP bg;
    flash::MC hint;
    bool hintOn = false;
    double mx = -1000, my = -1000;
    struct Hint {
        flash::MC mc;
        std::string html;
    };
    std::vector<Hint> hints;
    std::vector<flash::BMP> bitmaps;
    int current = -1;

    std::string fmt(const nlohmann::json& nameDesc, const std::vector<int>& args = {}) const;
    std::string item(int i, const std::vector<int>& args = {}) const;
    std::string misc(int i, const std::vector<int>& args = {}) const;
    void setHint(const flash::MC& mc, const std::string& html) { hints.push_back({mc, html}); }
    void grey(const flash::MC& mc);
    void showHint(const std::string& html);

    void initPad();
    void initDrones();
    void initMissiles();
    void initStars();
    void initCircle();
    void initPlanets();
    void initGadgets();
    void initEarthMap();
    void initPassengers();
};

}  // namespace site
