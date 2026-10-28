// Port of mt.bumdum (Motion Twin's sprite/particle helpers): Num, Col, Filt, Sprite, Phys, Part, Bmp, Plasma.
#pragma once
#include <functional>
#include <vector>

#include "Haxe.h"
#include "flash/Display.h"

namespace mt {

namespace Num {
inline double mm(double a, double b, double c) { return std::min(std::max(a, b), c); }
inline double sMod(double n, double mod) {
    if (mod == 0 || hx::isNull(mod) || hx::isNull(n)) return hx::NUL;
    while (n >= mod) n -= mod;
    while (n < 0) n += mod;
    return n;
}
inline double hMod(double n, double mod) {
    if (mod == 0 || hx::isNull(mod) || hx::isNull(n)) return hx::NUL;
    while (n > mod) n -= mod * 2;
    while (n < -mod) n += mod * 2;
    return n;
}
inline int rnd(int n, double f) { return hx::Int(std::pow(hx::random(), f) * n); }
}  // namespace Num

namespace Col {
struct RGB {
    int r, g, b;
};
inline RGB colToObj(int col) { return {(col >> 16) & 0xFF, (col >> 8) & 0xFF, col & 0xFF}; }
inline int objToCol(const RGB& o) { return (o.r << 16) | (o.g << 8) | o.b; }
// AS2 flash.Color.setTransform: ra..aa are percentages, rb..ab offsets. Also sets the clip's _alpha (aa).
void setTransform(flash::DisplayObject* mc, double ra, double ga, double ba, double aa, double rb, double gb, double bb,
                  double ab);
void setPercentColor(flash::DisplayObject* mc, double prc, int col, double inc = 0, double alpha = 100);
void setColor(flash::DisplayObject* mc, int col, double dec = -255);
RGB getRainbow(double c = hx::NUL);
int mergeCol(int col, int col2, double c = 0.5);
}  // namespace Col

namespace Filt {
void glow(flash::DisplayObject* mc, double bl = 2, double str = 10, int col = 0, bool inner = false);
void blur(flash::DisplayObject* mc, double blx = 0, double bly = 0);
}  // namespace Filt

class Sprite {
public:
    static std::vector<Sprite*> spriteList;
    static std::vector<Sprite*> graveyard;
    static void collectGarbage();  // free killed sprites (call when nothing references them anymore)

    explicit Sprite(flash::MC mc = nullptr);
    virtual ~Sprite() = default;

    double x = 0, y = 0;
    flash::MC root;
    double scale = 100;
    bool dead = false;

    void setScale(double n);
    virtual void update();
    virtual void kill();
    virtual void updatePos();
    double getDist(double ox, double oy) const;
    double getAng(double ox, double oy) const;
    void toward(double ox, double oy, double c, double lim = INFINITY);
};

class Phys : public Sprite {
public:
    explicit Phys(flash::MC mc = nullptr);
    double frict = hx::NUL;
    double vx = 0, vy = 0;
    double vr = hx::NUL, fr = hx::NUL, vsc = hx::NUL;
    double sleep = hx::NUL, timer = hx::NUL;
    double alpha = 100;
    double weight = hx::NUL;
    double fadeLimit = 10;
    int fadeType = -2;  // null
    void setAlpha(double n);
    void update() override;
    void towardSpeed(double ox, double oy, double c, double lim = INFINITY);
};

class Bmp;
class Part : public Phys {
public:
    enum Behaviour { HoriLine, VertiLine };
    explicit Part(flash::MC mc = nullptr) : Phys(std::move(mc)) {}
    std::vector<Behaviour> bhl;
    double coef = 1;
    Bmp* bmp = nullptr;  // optional trail bitmap
    void update() override;
};

// mt.bumdum.Bmp: a BitmapData attached to a clip at a quality factor
class Bmp {
public:
    Bmp(flash::MC mc, int w = 100, int h = 100, bool transparent = true, unsigned color = 0, double q = 1);
    virtual ~Bmp() = default;
    flash::MC root;
    flash::BMP bmp;
    double pq;
    void setPos(double x, double y) {
        root->x = x;
        root->y = y;
    }
    virtual void kill();
    void drawMc(flash::MovieClip* mc, double dx = 0, double dy = 0);
};

class Plasma : public Bmp {
public:
    static std::vector<Plasma*> list;
    Plasma(flash::MC mc, int w, int h, double q = 1);
    std::unique_ptr<flash::ColorTransform> ct;
    std::vector<flash::Filter> filters;
    double timer = hx::NUL;
    double fadeLimit = 10;
    void update();
    void kill() override;
    static void updateAll();
};

}  // namespace mt
