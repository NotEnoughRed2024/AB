// TextField: plain and (simple) HTML text rendered with the fonts embedded in the Flash library.
#include <algorithm>
#include <cmath>

#include "Display.h"
#include "Stage.h"

namespace flash {

TextField::TextField(Library* l, const EditTextDef& d) : DisplayObject(KText), lib(l), def(d) {
    textColor = d.color;
    textAlpha = d.alpha;
    width_ = d.bounds.width();
    height_ = d.bounds.height();
    if (d.html) setHtmlText(d.text);
    else setText(d.text);
}

void TextField::setText(const std::string& t) {
    if (!htmlMode && t == text && !dirty) return;
    htmlMode = false;
    text = t;
    spans.clear();
    spans.push_back({t, textColor, false, def.height});
    dirty = true;
}

static std::string decodeEntities(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '&') {
            size_t e = s.find(';', i);
            if (e != std::string::npos && e - i < 8) {
                std::string ent = s.substr(i + 1, e - i - 1);
                const char* rep = nullptr;
                if (ent == "amp") rep = "&";
                else if (ent == "lt") rep = "<";
                else if (ent == "gt") rep = ">";
                else if (ent == "quot") rep = "\"";
                else if (ent == "apos") rep = "'";
                else if (ent == "nbsp") rep = " ";
                if (rep) {
                    out += rep;
                    i = e;
                    continue;
                }
            }
        }
        out += s[i];
    }
    return out;
}

static std::string attr(const std::string& tag, const std::string& name) {
    std::string low = tag;
    std::transform(low.begin(), low.end(), low.begin(), ::tolower);
    size_t p = low.find(name + "=");
    if (p == std::string::npos) return "";
    p += name.size() + 1;
    char q = tag[p];
    if (q == '"' || q == '\'') {
        size_t e = tag.find(q, p + 1);
        return tag.substr(p + 1, e == std::string::npos ? std::string::npos : e - p - 1);
    }
    size_t e = tag.find_first_of(" >", p);
    return tag.substr(p, e == std::string::npos ? std::string::npos : e - p);
}

void TextField::setHtmlText(const std::string& src) {
    htmlMode = true;
    htmlSource = src;
    spans.clear();
    text.clear();
    struct St {
        unsigned color;
        bool bold;
        double size;
    };
    std::vector<St> stack{{textColor, false, def.height}};
    std::string cur;
    bool firstPara = true;
    auto flush = [&]() {
        if (!cur.empty()) {
            std::string d = decodeEntities(cur);
            spans.push_back({d, stack.back().color, stack.back().bold, stack.back().size});
            text += d;
            cur.clear();
        }
    };
    auto newline = [&]() {
        flush();
        spans.push_back({"\n", stack.back().color, stack.back().bold, stack.back().size});
        text += "\n";
    };
    for (size_t i = 0; i < src.size(); i++) {
        if (src[i] != '<') {
            cur += src[i];
            continue;
        }
        size_t e = src.find('>', i);
        if (e == std::string::npos) {
            cur += src.substr(i);
            break;
        }
        std::string tag = src.substr(i + 1, e - i - 1);
        i = e;
        std::string lname;
        for (char c : tag) {
            if (c == ' ' || c == '/') {
                if (!lname.empty() || c == ' ') break;
            }
            lname += (char)std::tolower((unsigned char)c);
        }
        if (lname == "p" || lname == "li") {
            if (!firstPara) newline();
            firstPara = false;
        } else if (lname == "/p" || lname == "/li") {
            flush();
        } else if (lname == "br" || lname == "br/") {
            newline();
        } else if (lname == "b") {
            flush();
            St s = stack.back();
            s.bold = true;
            stack.push_back(s);
        } else if (lname == "font") {
            flush();
            St s = stack.back();
            std::string c = attr(tag, "color");
            if (!c.empty() && c[0] == '#') s.color = (unsigned)std::strtoul(c.c_str() + 1, nullptr, 16);
            std::string sz = attr(tag, "size");
            if (!sz.empty()) s.size = std::atof(sz.c_str());
            stack.push_back(s);
        } else if (lname == "/b" || lname == "/font") {
            flush();
            if (stack.size() > 1) stack.pop_back();
        }
    }
    flush();
    dirty = true;
}

static unsigned nextCodepoint(const std::string& s, size_t& i) {
    unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    int n = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : 1;
    unsigned cp = c & (0x3F >> n);
    for (int k = 0; k < n && i < s.size(); k++) cp = (cp << 6) | ((unsigned char)s[i++] & 0x3F);
    return cp;
}

void TextField::layout() {
    dirty = false;
    runs.clear();
    const Font* base = lib->font(def.font);
    if (!base || !def.outlines) {
        const Font* f = lib->fontByName("Verdana", false);
        if (f) base = f;
    }
    if (!base) return;
    double boxX = def.bounds.xMin, boxW = width_;
    double left = boxX + 2 + def.leftMargin;
    double right = boxX + boxW - 2 - def.rightMargin;
    bool wrap = def.wordWrap;

    struct Item {
        int image;
        double adv, size;
        unsigned color;
        bool space, newline;
        const Font* font;
    };
    std::vector<Item> items;
    for (auto& sp : spans) {
        const Font* f = base;
        if (sp.bold) {
            if (const Font* bf = lib->fontByName(base->name, true)) f = bf;
        }
        for (size_t i = 0; i < sp.text.size();) {
            unsigned cp = nextCodepoint(sp.text, i);
            if (cp == '\r') cp = '\n';
            Item it{-1, 0, sp.size, sp.color, cp == ' ', cp == '\n', f};
            if (!it.newline) {
                auto g = f->byCode.find(cp);
                if (g == f->byCode.end() && cp >= 'a' && cp <= 'z') g = f->byCode.find(cp - 32);  // caps-only fonts
                if (g != f->byCode.end()) {
                    it.image = f->glyphs[g->second].image;
                    it.adv = f->glyphs[g->second].advance * sp.size;
                } else {
                    it.adv = (cp == ' ' ? 0.3 : 0.5) * sp.size;
                }
            }
            items.push_back(it);
        }
    }
    // break into lines
    std::vector<std::pair<size_t, size_t>> lines;
    size_t lineStart = 0;
    double x = 0;
    size_t lastSpace = std::string::npos;
    for (size_t i = 0; i < items.size(); i++) {
        if (items[i].newline) {
            lines.push_back({lineStart, i});
            lineStart = i + 1;
            x = 0;
            lastSpace = std::string::npos;
            continue;
        }
        if (items[i].space) lastSpace = i;
        x += items[i].adv;
        if (wrap && x > right - left && i > lineStart) {
            size_t brk = (lastSpace != std::string::npos && lastSpace > lineStart) ? lastSpace : i;
            lines.push_back({lineStart, brk});
            lineStart = (brk == i) ? i : brk + 1;
            x = 0;
            for (size_t k = lineStart; k <= i; k++) x += items[k].adv;
            lastSpace = std::string::npos;
        }
    }
    lines.push_back({lineStart, items.size()});

    double y = def.bounds.yMin + 2;
    textWidth = 0;
    for (auto [a, b] : lines) {
        double lsize = def.height;
        for (size_t k = a; k < b; k++) lsize = std::max(lsize, items[k].size);
        double asc = base->ascent * lsize, desc = base->descent * lsize;
        double w = 0;
        size_t end = b;
        while (end > a && items[end - 1].space) end--;
        for (size_t k = a; k < end; k++) w += items[k].adv;
        textWidth = std::max(textWidth, w);
        double lx = left;
        if (def.align == 1) lx = right - w;
        else if (def.align == 2) lx = left + (right - left - w) / 2;
        double baseline = y + asc;
        for (size_t k = a; k < end; k++) {
            if (items[k].image >= 0)
                runs.push_back({items[k].image, lx, baseline, items[k].size / items[k].font->emPx, items[k].color});
            lx += items[k].adv;
        }
        y += asc + desc + def.leading;
    }
    textHeight = y - def.bounds.yMin;
}

Rect TextField::localBounds() const {
    Rect r;
    r.add(def.bounds.xMin, def.bounds.yMin);
    r.add(def.bounds.xMin + width_, def.bounds.yMin + height_);
    return r;
}

void Stage::renderText(TextField* tf, const Matrix& m, const ColorTransform& ct, Blend blend) {
    if (tf->dirty) tf->layout();
    for (auto& r : tf->runs) {
        Matrix gm = m * Matrix::make(r.scale, 0, 0, r.scale, r.x, r.y);
        unsigned c = tf->htmlMode ? r.color : tf->textColor;
        ColorTransform cc(((c >> 16) & 255) / 255.0, ((c >> 8) & 255) / 255.0, (c & 255) / 255.0, tf->textAlpha);
        drawImage(tf->lib, r.image, gm, cc.concat(ct), blend);
    }
}

}  // namespace flash
