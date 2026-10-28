// Small HTML-subset text layout for the page around the game (mission feed, My Page, ...).
// Uses the Verdana glyphs embedded in the game's Flash library, so it needs no font files.
#pragma once
#include <string>
#include <vector>

#include "flash/Library.h"

namespace site {

struct TextStyle {
    unsigned color = 0xFFFFFF;
    double size = 11;  // page px
    bool bold = false;
    bool underline = false;
    bool smallCaps = false;
    int link = -1;  // >= 0: clickable, reported by RichText::linkAt
};

struct Span {
    std::string text;
    TextStyle style;
};

// Parses <p> <br> <strong>/<b> <em>/<i> <a href="N"> <font color=#RRGGBB>; everything else is ignored.
// strong/em/link take their look from the given styles (only colour/bold/underline are used).
struct HtmlTheme {
    TextStyle base, strong, em, link;
    double paraGap = 4;  // extra space between paragraphs
};
std::vector<Span> parseHtml(const std::string& html, const HtmlTheme& theme);

class RichText {
public:
    enum Align { Left, Right, Center };
    struct Placed {
        const flash::Font* font;
        int image;
        double x, y, scale;  // y = baseline; scale = page px per glyph unit
        unsigned color;
    };
    struct Box {
        double x, y, w, h;
        unsigned color;
        int link;
    };

    void set(std::vector<Span> spans) {
        spans_ = std::move(spans);
        laidWidth_ = -1;
    }
    void setPlain(const std::string& s, const TextStyle& st) { set({{s, st}}); }
    // wrapWidth <= 0: single line(s) without wrapping
    void layout(double wrapWidth, Align align = Left, double lineGap = 2);
    double width() const { return width_; }
    double height() const { return height_; }
    // draws at page position (x, y); u = window px per page px
    void draw(double x, double y, double u, double alpha = 1) const;
    int linkAt(double px, double py) const;  // relative to the text's origin

    static void setLibrary(flash::Library* lib) { lib_ = lib; }
    static double measure(const std::string& s, const TextStyle& st);

private:
    static flash::Library* lib_;
    std::vector<Span> spans_;
    std::vector<Placed> placed_;
    std::vector<Box> boxes_;  // underlines (h small) and link hit boxes (color 0)
    double laidWidth_ = -1, width_ = 0, height_ = 0;
    std::vector<double> paraGaps_;
};

}  // namespace site
