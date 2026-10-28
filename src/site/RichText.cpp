#include "RichText.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "flash/Stage.h"

namespace site {

flash::Library* RichText::lib_ = nullptr;

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

static std::string attrOf(const std::string& tag, const std::string& name) {
    size_t p = tag.find(name + "=");
    if (p == std::string::npos) return "";
    p += name.size() + 1;
    if (p >= tag.size()) return "";
    char q = tag[p];
    if (q == '"' || q == '\'') {
        size_t e = tag.find(q, p + 1);
        return tag.substr(p + 1, e == std::string::npos ? std::string::npos : e - p - 1);
    }
    size_t e = tag.find_first_of(" >/", p);
    return tag.substr(p, e == std::string::npos ? std::string::npos : e - p);
}

std::vector<Span> parseHtml(const std::string& html, const HtmlTheme& th) {
    std::vector<Span> out;
    std::vector<TextStyle> stack{th.base};
    std::string cur;
    bool anyText = false;
    auto flush = [&] {
        if (cur.empty()) return;
        // collapse whitespace like a browser
        std::string t;
        for (char c : cur) {
            if (c == '\n' || c == '\r' || c == '\t') c = ' ';
            if (c == ' ' && !t.empty() && t.back() == ' ') continue;
            t += c;
        }
        out.push_back({decodeEntities(t), stack.back()});
        anyText = true;
        cur.clear();
    };
    auto paraBreak = [&] {
        flush();
        if (anyText && !(!out.empty() && out.back().text == "\v")) {
            TextStyle gap = stack.back();
            gap.size = th.paraGap;
            out.push_back({"\v", gap});
        }
    };
    for (size_t i = 0; i < html.size(); i++) {
        if (html[i] != '<') {
            cur += html[i];
            continue;
        }
        size_t e = html.find('>', i);
        if (e == std::string::npos) {
            cur += html.substr(i);
            break;
        }
        std::string tag = html.substr(i + 1, e - i - 1);
        i = e;
        std::string name;
        for (char c : tag) {
            if (c == ' ' || (c == '/' && !name.empty())) break;
            name += (char)std::tolower((unsigned char)c);
        }
        auto push = [&](const TextStyle& look) {
            flush();
            TextStyle s = stack.back();
            s.color = look.color;
            s.bold = look.bold;
            s.underline = look.underline;
            stack.push_back(s);
        };
        if (name == "p") {
            paraBreak();
        } else if (name == "/p") {
            paraBreak();
        } else if (name == "br") {
            flush();
            out.push_back({"\n", stack.back()});
        } else if (name == "strong" || name == "b") {
            push(th.strong);
        } else if (name == "em" || name == "i") {
            push(th.em);
        } else if (name == "a") {
            push(th.link);
            stack.back().link = std::atoi(attrOf(tag, "href").c_str());
        } else if (name == "font") {
            flush();
            TextStyle s = stack.back();
            std::string c = attrOf(tag, "color");
            if (!c.empty() && c[0] == '#') s.color = (unsigned)std::strtoul(c.c_str() + 1, nullptr, 16);
            stack.push_back(s);
        } else if (name == "/strong" || name == "/b" || name == "/em" || name == "/i" || name == "/a" ||
                   name == "/font") {
            flush();
            if (stack.size() > 1) stack.pop_back();
        }
    }
    flush();
    while (!out.empty() && (out.back().text == "\v" || out.back().text.empty())) out.pop_back();
    // drop leading spaces of the first span and of spans following a break
    for (size_t k = 0; k < out.size(); k++) {
        bool afterBreak = k == 0 || out[k - 1].text == "\v" || out[k - 1].text == "\n";
        if (afterBreak) {
            auto& t = out[k].text;
            size_t n = t.find_first_not_of(' ');
            if (n == std::string::npos) t.clear();
            else t.erase(0, n);
        }
    }
    return out;
}

static unsigned nextCodepoint(const std::string& s, size_t& i) {
    unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    int n = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : 1;
    unsigned cp = c & (0x3F >> n);
    for (int k = 0; k < n && i < s.size(); k++) cp = (cp << 6) | ((unsigned char)s[i++] & 0x3F);
    return cp;
}

namespace {
struct Item {
    const flash::Font* font = nullptr;
    int image = -1;
    double adv = 0, size = 0;
    unsigned color = 0;
    bool space = false, newline = false, underline = false;
    double gap = 0;
    int link = -1;
};

const flash::Font* pickFont(flash::Library* lib, bool bold) {
    const flash::Font* f = lib->fontByName("Verdana", bold);
    if (!f) f = lib->fontByName("Verdana", false);
    return f;
}

unsigned upper(unsigned cp) {
    if (cp >= 'a' && cp <= 'z') return cp - 32;
    if (cp >= 0xE0 && cp <= 0xFE && cp != 0xF7) return cp - 32;
    return cp;
}

void itemsFor(flash::Library* lib, const Span& sp, std::vector<Item>& items) {
    const flash::Font* f = pickFont(lib, sp.style.bold);
    if (!f) return;
    if (sp.text == "\v") {
        Item it;
        it.newline = true;
        it.gap = sp.style.size;
        it.font = f;
        items.push_back(it);
        return;
    }
    for (size_t i = 0; i < sp.text.size();) {
        unsigned cp = nextCodepoint(sp.text, i);
        Item it;
        it.font = f;
        it.size = sp.style.size;
        it.color = sp.style.color;
        it.underline = sp.style.underline;
        it.link = sp.style.link;
        if (cp == '\n') {
            it.newline = true;
            items.push_back(it);
            continue;
        }
        if (cp == '\t') cp = ' ';
        it.space = cp == ' ';
        if (sp.style.smallCaps && upper(cp) != cp) {
            cp = upper(cp);
            it.size *= 0.78;
        }
        auto g = f->byCode.find(cp);
        if (g == f->byCode.end() && cp == 0x2019) g = f->byCode.find('\'');
        if (g != f->byCode.end()) {
            it.image = f->glyphs[g->second].image;
            it.adv = f->glyphs[g->second].advance * it.size;
        } else {
            it.adv = (cp == ' ' ? 0.35 : 0.6) * it.size;
        }
        items.push_back(it);
    }
}
}  // namespace

double RichText::measure(const std::string& s, const TextStyle& st) {
    if (!lib_) return 0;
    std::vector<Item> items;
    itemsFor(lib_, {s, st}, items);
    double w = 0;
    for (auto& it : items) w += it.adv;
    return w;
}

void RichText::layout(double wrapWidth, Align align, double lineGap) {
    placed_.clear();
    boxes_.clear();
    width_ = height_ = 0;
    laidWidth_ = wrapWidth;
    if (!lib_) return;
    std::vector<Item> items;
    for (auto& sp : spans_) itemsFor(lib_, sp, items);

    std::vector<std::pair<size_t, size_t>> lines;
    size_t lineStart = 0, lastSpace = std::string::npos;
    double x = 0;
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
        if (wrapWidth > 0 && x > wrapWidth && i > lineStart) {
            size_t brk = (lastSpace != std::string::npos && lastSpace > lineStart) ? lastSpace : i;
            lines.push_back({lineStart, brk});
            lineStart = (brk == i) ? i : brk + 1;
            x = 0;
            for (size_t k = lineStart; k <= i; k++) x += items[k].adv;
            lastSpace = std::string::npos;
        }
    }
    lines.push_back({lineStart, items.size()});

    const flash::Font* base = pickFont(lib_, false);
    double y = 0;
    for (size_t li = 0; li < lines.size(); li++) {
        auto [a, b] = lines[li];
        // skip leading spaces on wrapped lines
        while (a < b && items[a].space) a++;
        double lsize = 0;
        for (size_t k = a; k < b; k++) lsize = std::max(lsize, items[k].size);
        bool empty = lsize == 0;
        if (empty) {
            // an empty line: a paragraph gap or a <br><br>
            double gap = (b < items.size() && items[b].newline) ? items[b].gap : 0;
            if (gap == 0 && li > 0 && b < items.size()) {
                // plain blank line: the height of the previous span's text
                for (size_t k = b; k-- > 0;)
                    if (items[k].size > 0) {
                        lsize = items[k].size;
                        break;
                    }
            }
            y += gap + (lsize > 0 ? lsize * (base->ascent + base->descent) + lineGap : 0);
            continue;
        }
        double asc = base->ascent * lsize, desc = base->descent * lsize;
        size_t end = b;
        while (end > a && items[end - 1].space) end--;
        double w = 0;
        for (size_t k = a; k < end; k++) w += items[k].adv;
        width_ = std::max(width_, w);
        double lx = 0;
        if (wrapWidth > 0 && align == Right) lx = wrapWidth - w;
        else if (wrapWidth > 0 && align == Center) lx = (wrapWidth - w) / 2;
        double baseline = y + asc;
        for (size_t k = a; k < end; k++) {
            const Item& it = items[k];
            if (it.image >= 0) placed_.push_back({it.font, it.image, lx, baseline, it.size / it.font->emPx, it.color});
            if (it.underline) boxes_.push_back({lx, baseline + it.size * 0.12, it.adv, std::max(1.0, it.size / 11), it.color, -1});
            if (it.link >= 0) boxes_.push_back({lx, y, it.adv, asc + desc, 0, it.link});
            lx += it.adv;
        }
        y += asc + desc + lineGap;
        // a paragraph gap after this line
        if (b < items.size() && items[b].newline) y += items[b].gap;
    }
    height_ = y;
}

void RichText::draw(double x, double y, double u, double alpha) const {
    auto* st = flash::Stage::get();
    SDL_Renderer* r = st->renderer;
    for (auto& p : placed_) {
        double s = p.scale * u;
        flash::Matrix m = flash::Matrix::make(s, 0, 0, s, (x + p.x) * u, (y + p.y) * u);
        flash::ColorTransform ct(((p.color >> 16) & 255) / 255.0, ((p.color >> 8) & 255) / 255.0,
                                 (p.color & 255) / 255.0, alpha);
        st->drawImage(lib_, p.image, m, ct, flash::Blend::Normal);
    }
    for (auto& b : boxes_) {
        if (b.link >= 0) continue;
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, (b.color >> 16) & 255, (b.color >> 8) & 255, b.color & 255, (Uint8)(alpha * 255));
        SDL_FRect fr{(float)((x + b.x) * u), (float)std::floor((y + b.y) * u), (float)(b.w * u),
                     (float)std::max(1.0, std::floor(b.h * u))};
        SDL_RenderFillRect(r, &fr);
    }
}

int RichText::linkAt(double px, double py) const {
    for (auto& b : boxes_)
        if (b.link >= 0 && px >= b.x && px < b.x + b.w && py >= b.y && py < b.y + b.h) return b.link;
    return -1;
}

}  // namespace site
