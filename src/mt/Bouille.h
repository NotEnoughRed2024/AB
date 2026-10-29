// mt.bumdum.Bouille: dresses a "skinnable" clip from a list of numbers. Child clips named p<N> show
// frame skin[firstDecal+N]; p<r> a random frame; col<NN> (at name position 0 or 2) take a palette colour.
#pragma once
#include <string>
#include <vector>

#include "Bumdum.h"
#include "Haxe.h"

namespace mt {

class Bouille {
public:
    int colorDecal = 5, firstDecal = 1;
    std::vector<std::vector<int>> palette{{0xFF0000, 0x00FF00, 0x0000FF, 0x00FFFF, 0xFFFF00, 0xFF00FF}};
    std::vector<int> skin;
    std::vector<int> paletteRedirect;

    void apply(flash::MovieClip* mc) {
        framize(mc);
        colorize(mc);
    }

    void framize(flash::MovieClip* root) {
        for (auto& [d, o] : root->children) {
            if (o->kind != flash::DisplayObject::KClip) continue;
            auto* mc = static_cast<flash::MovieClip*>(o.get());
            std::string str = o->name;
            if (!str.empty() && str[0] == '_') str = str.substr(1);
            if (!str.empty() && str[0] == 'p') {
                std::string st = str.substr(1, 1);
                if (st == "r") mc->gotoAndStop(hx::random(mc->totalFrames()) + 1);
                else if (!st.empty() && st[0] >= '0' && st[0] <= '9') setFrame(mc, st[0] - '0');
                // Std.parseInt of a non-digit is null: gotoAndStop(NaN) leaves the clip alone
            }
            framize(mc);
        }
    }

    void colorize(flash::MovieClip* root) {
        for (auto& [d, o] : root->children) {
            if (o->kind != flash::DisplayObject::KClip) continue;
            auto* mc = static_cast<flash::MovieClip*>(o.get());
            std::string str = o->name;
            if (!str.empty() && str[0] == '_') str = str.substr(1);
            for (int i = 0; i < 2; i++) {
                if (str.size() >= (size_t)(i * 2 + 3) && str.compare(i * 2, 3, "col") == 0) {
                    std::string num = str.substr(3 + i * 2, 2);
                    if (!num.empty() && num[0] >= '0' && num[0] <= '9') setCol(mc, std::atoi(num.c_str()));
                }
            }
            colorize(mc);
        }
    }

    void setCol(flash::MovieClip* mc, int n) {
        int pid = n;
        if (!paletteRedirect.empty()) pid = (n >= 0 && n < (int)paletteRedirect.size()) ? paletteRedirect[n] : -1;
        const std::vector<int>* pal = (pid >= 0 && pid < (int)palette.size()) ? &palette[pid] : &palette[0];
        n += colorDecal + firstDecal;
        if (n < 0 || n >= (int)skin.size() || pal->empty()) return;  // undefined colour: setColor is a no-op
        Col::setColor(mc, (*pal)[skin[n] % pal->size()]);
    }

    void setFrame(flash::MovieClip* mc, int n) {
        int i = firstDecal + n;
        if (i < 0 || i >= (int)skin.size()) return;
        mc->gotoAndStop(skin[i] % mc->totalFrames() + 1);
    }
};

}  // namespace mt
