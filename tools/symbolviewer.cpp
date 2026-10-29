// Debug tool: shows exported symbols from the converted art library.
//   symbolviewer <assets dir> [--shot out.png] [--frames N] name[:frame][@x,y] ...
// With --shot it renders headlessly after N frames and writes a PNG.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <string>
#include <vector>

#include "Stage.h"
#include "stb_image_write.h"

using namespace flash;

static bool saveShot(SDL_Renderer* r, const char* path) {
    SDL_Surface* s = SDL_RenderReadPixels(r, nullptr);
    if (!s) return false;
    SDL_Surface* c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(s);
    if (!c) return false;
    int ok = stbi_write_png(path, c->w, c->h, 4, c->pixels, c->pitch);
    SDL_DestroySurface(c);
    return ok != 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        SDL_Log("usage: symbolviewer <assets dir> [--shot out.png] [--frames N] name[:frame][@x,y] ...");
        return 1;
    }
    std::string dir = argv[1];
    const char* shot = nullptr;
    int frames = 1;
    std::string libName = "gfx";
    std::vector<std::string> names;
    for (int i = 2; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--shot" && i + 1 < argc) shot = argv[++i];
        else if (a == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (a == "--lib" && i + 1 < argc) libName = argv[++i];
        else names.push_back(a);
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }
    SDL_Window* win = SDL_CreateWindow("symbolviewer", 920, 720, shot ? SDL_WINDOW_HIDDEN : 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    Stage stage(win, ren, 460, 360, 40);
    if (!stage.loadLibrary(dir, libName)) return 1;
    auto bg = stage.root->createEmptyMovieClip("bg", 0);
    int d = 1;
    double x = 40, y = 40;
    for (auto& n : names) {
        std::string nm = n;
        int fr = 0;
        double px = x, py = y;
        size_t at = nm.find('@');
        if (at != std::string::npos) {
            sscanf(nm.c_str() + at + 1, "%lf,%lf", &px, &py);
            nm = nm.substr(0, at);
        }
        size_t c = nm.find(':');
        if (c != std::string::npos) {
            fr = std::atoi(nm.c_str() + c + 1);
            nm = nm.substr(0, c);
        }
        auto mc = stage.root->attachMovie(nm, nm, d++);
        mc->x = px;
        mc->y = py;
        if (fr > 0) mc->gotoAndStop(fr);
        SDL_Log("%s: frames=%d bounds w=%.1f h=%.1f", nm.c_str(), mc->totalFrames(), mc->width(), mc->height());
        x += 70;
        if (x > 360) {
            x = 40;
            y += 60;
        }
    }
    bool run = true;
    int n = 0;
    Uint64 last = SDL_GetTicks();
    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) run = false;
            stage.handleEvent(e);
        }
        stage.tick();
        stage.render();
        if (shot && ++n >= frames) {
            bool ok = saveShot(ren, shot);
            SDL_Log("screenshot %s: %s", shot, ok ? "ok" : SDL_GetError());
            break;
        }
        SDL_RenderPresent(ren);
        Uint64 now = SDL_GetTicks();
        if (now - last < 25) SDL_Delay((Uint32)(25 - (now - last)));
        last = SDL_GetTicks();
    }
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
