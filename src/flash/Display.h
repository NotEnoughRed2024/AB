// A small re-implementation of the Flash 8 (AS2) display list: MovieClip, TextField, BitmapData.
// Only what Alphabounce uses is implemented.
#pragma once
#include <SDL3/SDL.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Geom.h"
#include "Library.h"

namespace flash {

class MovieClip;
class TextField;
class BitmapData;
class Stage;
using MC = std::shared_ptr<MovieClip>;
using TF = std::shared_ptr<TextField>;
using BMP = std::shared_ptr<BitmapData>;

class DisplayObject : public std::enable_shared_from_this<DisplayObject> {
public:
    enum Kind { KClip, KShape, KText, KBitmap };
    explicit DisplayObject(Kind k) : kind(k) {}
    virtual ~DisplayObject() = default;

    Kind kind;
    MovieClip* parent = nullptr;
    int depth = 0;
    std::string name;
    bool removed = false;

    // transform (AS2 style)
    double x = 0, y = 0, xscale = 100, yscale = 100, rotation = 0, skew = 0;
    double alpha = 100;
    bool visible = true;
    ColorTransform color;  // from Color/transform.colorTransform (alpha multiplier is _alpha)
    std::vector<Filter> filters;
    Blend blendMode = Blend::Normal;
    bool cacheAsBitmap = false;

    // timeline bookkeeping
    int timelineInstance = 0;  // >0 when placed by a timeline
    bool scripted = false;     // script touched the transform -> timeline no longer moves it
    int clipDepth = 0;         // mask layer

    Matrix matrix() const;
    void setMatrix(const Matrix& m);
    ColorTransform colorTransform() const;  // including _alpha
    Matrix concatenatedMatrix() const;       // to stage logical coordinates

    // AS2 helpers
    void setX(double v) { x = v; scripted = true; }
    void setY(double v) { y = v; scripted = true; }
    void setXScale(double v) { xscale = v; scripted = true; }
    void setYScale(double v) { yscale = v; scripted = true; }
    void setRotation(double v);
    void setAlpha(double v) { alpha = v; scripted = true; }

    virtual Rect localBounds() const { return Rect(); }  // in own coordinate space
    Rect getBounds(const DisplayObject* target) const;
    double width() const;
    double height() const;
    void setWidth(double w);
    void setHeight(double h);

    Point localToGlobal(Point p) const;
    Point globalToLocal(Point p) const;

    void removeMovieClip();
    void swapDepths(int newDepth);
    int getDepth() const { return depth; }
};

class Shape : public DisplayObject {
public:
    Shape(Library* l, int img, int chr = -1) : DisplayObject(KShape), lib(l), image(img), charId(chr) {}
    Library* lib;
    int image;
    int charId;
    Rect localBounds() const override { return lib->imageBounds(image); }
};

class BitmapHolder : public DisplayObject {
public:
    explicit BitmapHolder(BMP b) : DisplayObject(KBitmap), bmp(std::move(b)) {}
    BMP bmp;
    Rect localBounds() const override;
};

class TextField : public DisplayObject {
public:
    TextField(Library* l, const EditTextDef& d);
    Library* lib;
    EditTextDef def;
    std::string text;      // plain text (UTF-8)
    bool htmlMode = false;
    std::string htmlSource;
    unsigned textColor = 0;
    double textAlpha = 1;
    double width_ = 100, height_ = 20;
    bool selectable = false;

    void setText(const std::string& t);
    void setHtmlText(const std::string& t);
    const std::string& getText() const { return text; }
    Rect localBounds() const override;

    // layout cache
    struct Run {
        int image;
        double x, y, scale;
        unsigned color;
    };
    std::vector<Run> runs;
    bool dirty = true;
    double textWidth = 0, textHeight = 0;
    void layout();
    struct Span {
        std::string text;
        unsigned color;
        bool bold;
        double size;
    };
    std::vector<Span> spans;
};

class MovieClip : public DisplayObject {
public:
    MovieClip(Library* l, const Character* def);
    ~MovieClip() override;

    Library* lib;
    const Character* def;  // nullptr for empty clips
    int frame = 1;         // 1-based current frame
    bool playing = true;
    std::map<int, std::shared_ptr<DisplayObject>> children;

    // events
    std::function<void()> onPress, onRelease, onReleaseOutside, onRollOver, onRollOut, onDragOver, onDragOut;
    std::function<void()> onEnterFrame;
    std::function<void()> onObjKill;  // "obj.kill()" called by some frame scripts
    bool useHandCursor = true;
    bool enabled = true;
    MC hitArea;

    // free-form per-clip fields used by game code (mc.c, mc.timer, ...)
    std::unordered_map<std::string, double> vars;
    double& var(const std::string& k) { return vars[k]; }

    int totalFrames() const { return def ? (int)def->frames.size() : 1; }
    int currentFrame() const { return frame; }
    void gotoAndStop(int f);
    void gotoAndStop(const std::string& label);
    void gotoAndPlay(int f);
    void gotoAndPlay(const std::string& label);
    void play() { playing = true; }
    void stop() { playing = false; }
    void nextFrame() { gotoAndStop(frame + 1); }
    void prevFrame() { gotoAndStop(frame - 1); }

    MC attachMovie(const std::string& linkage, const std::string& instName, int depth);
    MC createEmptyMovieClip(const std::string& instName, int depth);
    void attachBitmap(BMP bmp, int depth);
    TF createTextField(const std::string& instName, int depth, double x, double y, double w, double h);

    std::shared_ptr<DisplayObject> child(const std::string& n) const;
    MC clip(const std::string& n) const;      // child as MovieClip (nullptr if missing)
    TF field(const std::string& n) const;     // child as TextField
    MC smc() const { return clip("smc"); }
    void removeChildAt(int depth);
    void addChild(std::shared_ptr<DisplayObject> d, int depth);
    int nextHighestDepth() const;

    // flash drawing API (lines only - all the game uses)
    struct Stroke {
        double thickness = 0;
        unsigned color = 0;
        double alpha = 1;
        std::vector<Point> pts;
    };
    std::vector<Stroke> strokes;
    Stroke pen;
    bool penSet = false;
    void clear() { strokes.clear(); penSet = false; pen.pts.clear(); }
    void lineStyle(double thickness, unsigned color = 0, double alpha = 100);
    void moveTo(double x, double y);
    void lineTo(double x, double y);

    // MovieClipLoader.loadClip: become another library's main timeline
    void loadRoot(Library* other);

    Rect localBounds() const override;
    bool hitTest(double gx, double gy, bool shapeFlag = false) const;
    bool hitTestClip(const MovieClip* other) const;

    Point mouse() const;  // _xmouse/_ymouse

    // timeline
    void advance();  // called once per frame by Stage
    void runFrameActions();
    bool constructed = false;

private:
    void applyFrame(int f);
    std::shared_ptr<DisplayObject> instantiate(const Placement& p);
};

// BitmapData lives on the GPU as a render target; pixel-level operations read it back.
class BitmapData {
public:
    BitmapData(Stage* s, int w, int h, bool transparent, unsigned fillARGB, bool soft = false);
    // soft bitmaps live in CPU memory at 1:1 and rasterise vector shapes exactly like Flash's
    // 4x4 supersampled renderer; the game uses them where it reads pixels back (level generation).
    bool soft = false;
    std::vector<uint32_t> cpu;  // ARGB (not premultiplied), soft bitmaps only
    std::vector<uint8_t> softMask;  // optional: only rasterise pixels whose mask byte is set
    unsigned getPixel(int x, int y) { return getPixel32(x, y) & 0xFFFFFF; }
    ~BitmapData();
    int width, height;
    bool transparent;
    double res;  // texture pixels per logical pixel
    SDL_Texture* texture = nullptr;
    Stage* stage;

    void fillRect(Rect r, unsigned argb);
    void draw(DisplayObject* src, const Matrix& m, const ColorTransform* ct = nullptr, Blend blend = Blend::Normal);
    void draw(BitmapData* src, const Matrix& m, const ColorTransform* ct = nullptr, Blend blend = Blend::Normal);
    void applyBlur(double bx, double by, int quality = 1);
    void applyFilter(const Filter& f);
    void colorTransform(const ColorTransform& ct);
    void perlinNoise(double baseX, double baseY, int octaves, int seed, bool stitch, bool fractal, int channels,
                     bool grey);
    void dispose();
    unsigned getPixel32(int x, int y);
    void setPixel32(int x, int y, unsigned argb);
    Rect rectangle() const {
        Rect r;
        r.add(0, 0);
        r.add(width, height);
        return r;
    }
    // CPU round trip helpers
    std::vector<uint32_t> readPixels();          // ARGB, texture resolution
    void writePixels(const std::vector<uint32_t>& px);  // ARGB, texture resolution
    int texW() const { return tw; }
    int texH() const { return th; }

private:
    int tw, th;
};

}  // namespace flash
