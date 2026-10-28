// Loads a converted Flash library (atlas pages + JSON written by tools/swfconv/convert.py).
#pragma once
#include <SDL3/SDL.h>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Geom.h"

namespace flash {

struct Image {
    int page = 0;
    int x = 0, y = 0, w = 0, h = 0;  // atlas pixels
    double ox = 0, oy = 0;           // atlas pixel of the shape's origin (relative to x,y)
};

struct Filter {
    enum Type { Glow, Blur, Shadow, ColorMatrix } type = Glow;
    double blurX = 0, blurY = 0, strength = 1, angle = 0, distance = 0;
    unsigned color = 0;  // 0xRRGGBB
    double alpha = 1;
    bool inner = false, knockout = false;
    int quality = 1;
    double matrix[20] = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0};

    static Filter glow(double blur, double strength, unsigned color, bool inner = false) {
        Filter f;
        f.type = Glow;
        f.blurX = f.blurY = blur;
        f.strength = strength;
        f.color = color;
        f.inner = inner;
        return f;
    }
    static Filter blur(double bx, double by) {
        Filter f;
        f.type = Blur;
        f.blurX = bx;
        f.blurY = by;
        return f;
    }
    static Filter colorMatrix(const double* m) {
        Filter f;
        f.type = ColorMatrix;
        for (int i = 0; i < 20; i++) f.matrix[i] = m[i];
        return f;
    }
};

enum class Blend { Normal, Add, Subtract, Multiply, Screen, Lighten, Darken, Difference, Overlay, Hardlight, Invert, Alpha, Erase };
Blend blendFromString(const std::string& s);

struct Placement {
    int character = 0;
    int instance = 0;  // timeline instance id (changes when a new object is placed at this depth)
    int depth = 0;
    Matrix matrix;
    bool hasColor = false;
    ColorTransform color;
    std::string name;
    int clipDepth = 0;
    std::vector<Filter> filters;
    Blend blend = Blend::Normal;
};

enum class FrameAction { Stop, Play, RandomFrame, RemoveSelf, CallObjKill, RandomRotation, CounterParentRotation, Rotate2, GotoAndPlay1, Goto };

struct Frame {
    std::vector<Placement> displayList;
    std::vector<std::pair<FrameAction, int>> actions;
    std::string label;
};

struct EditTextDef {
    Rect bounds;
    int font = 0;
    double height = 12;
    unsigned color = 0;
    double alpha = 1;
    int align = 0;  // 0 left 1 right 2 center 3 justify
    double leftMargin = 0, rightMargin = 0, indent = 0, leading = 0;
    bool wordWrap = false, multiline = false, html = false, outlines = false, autoSize = false, border = false;
    std::string text, variable;
};

struct Glyph {
    unsigned code = 0;
    double advance = 0;  // in em
    int image = -1;
};

struct Font {
    std::string name;
    bool bold = false, italic = false;
    double emPx = 32;  // nominal em size the glyph images were rasterised at (logical px)
    double ascent = 0.9, descent = 0.25, leading = 0;
    std::vector<Glyph> glyphs;
    std::unordered_map<unsigned, int> byCode;
};

// Vector outlines (only for shapes the game rasterises on the CPU, e.g. level-generation brushes)
struct VecPaint {
    enum Type { Solid, Linear, Radial } type = Solid;
    double r = 0, g = 0, b = 0, a = 255;
    Matrix matrix;  // gradient space -> shape space (px)
    std::vector<std::pair<double, std::array<double, 4>>> stops;  // ratio 0..255, rgba
    int spread = 0;
    void colorAt(double x, double y, double out[4]) const;
};
struct VecFill {
    VecPaint paint;
    std::vector<std::vector<double>> polys;  // closed polygons, flat x,y (px)
    Rect bounds;
};

struct Character {
    enum Kind { None, Shape, Bitmap, Sprite, EditText, FontRef } kind = None;
    int image = -1;                 // Shape / Bitmap
    int bitmapW = 0, bitmapH = 0;   // Bitmap
    std::vector<Frame> frames;      // Sprite
    std::map<std::string, int> labels;
    EditTextDef edit;               // EditText
};

class Library {
public:
    bool load(SDL_Renderer* renderer, const std::string& dir, const std::string& name);
    void unload();

    const Character* character(int id) const;
    int exportId(const std::string& linkage) const;
    const Font* font(int id) const;
    const Font* fontByName(const std::string& name, bool bold) const;
    const Image& image(int i) const { return images[i]; }
    SDL_Texture* page(int i) const { return pages[i]; }
    SDL_Texture* silhouettePage(int i);  // white copy of a page (alpha only), made on demand
    double scale = 2;                     // atlas pixels per logical pixel
    std::vector<Frame> rootFrames;
    std::string name;

    const std::vector<VecFill>* vectorShape(int charId) const {
        auto it = vectors.find(charId);
        return it == vectors.end() ? nullptr : &it->second;
    }

    // logical-pixel bounds of an image (shape space)
    Rect imageBounds(int i) const;

private:
    SDL_Renderer* renderer = nullptr;
    std::vector<SDL_Texture*> pages;
    std::vector<SDL_Texture*> silhouettes;
    std::vector<Image> images;
    std::unordered_map<int, Character> chars;
    std::unordered_map<std::string, int> exports;
    std::unordered_map<int, Font> fonts;
    std::unordered_map<int, std::vector<VecFill>> vectors;
};

}  // namespace flash
