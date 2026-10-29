// The alphabounce.com page around the game: header, game panel with its control strip, and the
// right-hand panel (Missions feed, My Page, Records, Help). The game itself is drawn by flash::Stage
// into the rectangle this class lays out.
#pragma once
#include <SDL3/SDL.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Inventory.h"
#include "RichText.h"
#include "json.hpp"

namespace site {

struct Tex {
    SDL_Texture* t = nullptr;
    int w = 0, h = 0;
};

class Site {
public:
    Site() = default;
    ~Site();
    bool init(SDL_Renderer* r, const std::string& siteDir, const std::string& dataDir, const std::string& lang);

    // lay out the page for the current window size; tells the stage where the game goes
    void layout();
    void render();  // the page (call before Stage::render(false))
    void handleEvent(const SDL_Event& e);  // call after Stage::handleEvent
    void update();  // once per frame: refresh the panel when the player's data changed

    enum Tab { Missions, MyPage, Records, Help, Debug, TabCount };
    void setTab(int t);
    int tab() const { return tab_; }

private:
    struct Card {
        Tex* icon = nullptr;
        Tex* vig = nullptr;
        RichText title, date, body;
        bool info = false;  // the "if you are lost" style box
        double head = 22;   // header height (34 when the date goes under the title)
        bool compact = false;  // body follows the title without the vignette column
        bool widget = false;   // the inventory widget
        double y = 0, h = 0;
    };
    struct R {
        double x = 0, y = 0, w = 0, h = 0;
        bool has(double px, double py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    };

    SDL_Renderer* ren = nullptr;
    std::string dir, lang;
    nlohmann::json texts;
    std::map<std::string, Tex> tex;
    Tex* load(const std::string& file, bool optional = false);
    Tex* vignette(int mission);
    std::string t(const std::string& key) const;

    // layout (page px; one page px = u window px)
    double u = 1, PW = 900, PH = 500;
    R game, strip, panel, content, tabsR[TabCount], helpBtn, scrollTrack, scrollThumb;
    bool hasThumb = false;

    // panel content
    int tab_ = Missions;
    std::vector<Card> cards;
    double contentH = 0, scrollY = 0;
    double builtWidth = -1;
    std::string builtSig;
    bool dragThumb = false;
    double dragOffset = 0;
    double mx = -1, my = -1;  // pointer, page px
    bool pressed = false;
    SDL_Cursor* hand = nullptr;
    SDL_Cursor* arrow = nullptr;
    bool handShown = false;

    RichText tabLabel[TabCount], stripText[3], helpLabel, minerals;
    Inventory inventory;
    R widgetR;  // where the inventory was drawn (page px), for the pointer

    std::string signature() const;
    void rebuild();
    void buildMissions();
    void buildMyPage();
    void buildRecords();
    void buildHelp();
    void buildDebug();
    Card& addCard(Tex* icon, const std::string& title, const std::string& html, const std::string& date = "",
                  Tex* vig = nullptr);
    std::string fillPlaceholders(std::string s) const;
    std::string formatDate(long long t) const;
    void clampScroll();

    // drawing helpers (page px)
    void blit(Tex* tx, double sx, double sy, double sw, double sh, double dx, double dy, double dw, double dh,
              double alpha = 1);
    void blit(Tex* tx, double dx, double dy, double alpha = 1) { if (tx) blit(tx, 0, 0, tx->w, tx->h, dx, dy, tx->w, tx->h, alpha); }
    void nine(Tex* tx, double sx, double sy, double sw, double sh, double l, double r, double tp, double b, const R& d);
    void fill(const R& r, unsigned rgb, double alpha);
    void clip(const R* r);
    void drawFrame();
    void drawPanel();
};

}  // namespace site
