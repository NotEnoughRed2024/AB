// Alphabounce SDL3 port - entry point.
//
// Options (mostly for testing):
//   --lang en|fr|de|es    text language (default en)
//   --assets DIR          folder holding assets/ and data/ (default: next to the executable, then ".")
//   --play X,Y            start straight into sector X,Y
//   --autoplay            let the pad play by itself (the original's PLAY_AUTO mode)
//   --frames N            quit after N frames
//   --shot FILE           save a PNG of the last frame (with --frames)
//   --scale N             window scale (default: fit the screen)
//   --fullscreen          start in full screen (F11 or Alt+Enter toggles)
//   --bare                just the game, without the page around it
//   --tab N               open the page on tab N (0 missions, 1 my page, 2 records, 3 help)
//   --size WxH            window size in pixels (testing)
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <memory>
#include <string>

#include "flash/Stage.h"
#include "game/Shell.h"
#include "site/Site.h"
#include "stb_image_write.h"

static bool fileExists(const std::string& p) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(p.c_str(), &info);
}

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
    std::string lang = "en", base, shot;
    int frames = -1, tab = 0, winW = 0, winH = 0;
    double scale = 0;
    bool autoplay = false, directPlay = false, fullscreen = false, bare = false;
    int playX = 0, playY = 0;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--lang") lang = next();
        else if (a == "--assets") base = next();
        else if (a == "--frames") frames = std::atoi(next().c_str());
        else if (a == "--shot") shot = next();
        else if (a == "--scale") scale = std::max(0.5, std::atof(next().c_str()));
        else if (a == "--fullscreen") fullscreen = true;
        else if (a == "--bare") bare = true;
        else if (a == "--tab") tab = std::atoi(next().c_str());
        else if (a == "--size") std::sscanf(next().c_str(), "%dx%d", &winW, &winH);
        else if (a == "--autoplay") autoplay = true;
        else if (a == "--play") {
            directPlay = true;
            std::sscanf(next().c_str(), "%d,%d", &playX, &playY);
        }
    }
    if (base.empty()) {
        const char* bp = SDL_GetBasePath();
        base = bp ? bp : "./";
        if (!fileExists(base + "assets/gfx.json")) base = "./";
    }
    if (!base.empty() && base.back() != '/' && base.back() != '\\') base += "/";

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    SDL_SetAppMetadata("Alphabounce", "0.1", "org.fans.alphabounce");
    bool headless = !shot.empty();
    // the page is 900x500 (the game alone 400x360); by default fill most of the screen
    double pageW = bare ? 400 : 900, pageH = bare ? 360 : 500;
    if (scale <= 0) {
        scale = 2;
        SDL_Rect usable;
        if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable) && usable.w > 0)
            scale = std::max(1.0, std::min(usable.w * 0.9 / pageW, usable.h * 0.85 / pageH));
    }
    if (winW <= 0 || winH <= 0) {
        winW = (int)(pageW * scale);
        winH = (int)(pageH * scale);
    }
    SDL_Window* win = SDL_CreateWindow("Alphabounce", winW, winH,
                                       SDL_WINDOW_RESIZABLE | (headless ? SDL_WINDOW_HIDDEN : 0) |
                                           (fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
    if (!win) {
        SDL_Log("window: %s", SDL_GetError());
        return 1;
    }
    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        SDL_Log("renderer: %s", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(ren, 1);

    int rc = 0;
    {
        flash::Stage stage(win, ren, 400, 360, 40);
        if (!stage.loadLibrary(base + "assets", "gfx")) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Alphabounce",
                                     "Could not load assets/gfx.json - see README for how to generate the assets.", win);
            return 1;
        }
        Shell shell;
        if (!shell.init(base + "assets", base + "data", lang)) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Alphabounce", "Could not load data/game_data.json.", win);
            return 1;
        }
        std::unique_ptr<site::Site> page;
        if (!bare) {
            page = std::make_unique<site::Site>();
            if (!page->init(ren, base + "assets/site", base + "data", lang)) {
                SDL_Log("page graphics missing - running the game alone");
                page.reset();
            } else {
                page->setTab(tab);
            }
        }
        Game::PLAY_AUTO = autoplay;
        stage.root->onEnterFrame = [&] { shell.update(); };
        if (directPlay) shell.playDirect(playX, playY);

        const double frameMs = 1000.0 / stage.fps;
        Uint64 last = SDL_GetTicksNS();
        double acc = 0;
        int count = 0;
        bool run = true;
        while (run) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) run = false;
                if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat &&
                    (e.key.key == SDLK_F11 || (e.key.key == SDLK_RETURN && (e.key.mod & SDL_KMOD_ALT)))) {
                    SDL_SetWindowFullscreen(win, !(SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN));
                    continue;
                }
                if (page) page->layout();  // input mapping needs the current layout
                stage.handleEvent(e);
                if (page) page->handleEvent(e);
            }
            Uint64 now = SDL_GetTicksNS();
            acc += (now - last) / 1e6;
            last = now;
            if (headless) acc = frameMs;  // as fast as possible when testing
            int steps = 0;
            while (acc >= frameMs && steps < 4) {
                stage.tick();
                acc -= frameMs;
                steps++;
                count++;
                if (frames > 0 && count >= frames) run = false;
            }
            if (acc > frameMs * 4) acc = 0;
            if (shell.quitRequested) run = false;
            if (page) {
                page->update();
                page->layout();
                SDL_SetRenderTarget(ren, nullptr);
                SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
                SDL_RenderClear(ren);
                stage.currentTarget = nullptr;
                page->render();
                stage.render(false);
            } else {
                stage.render();
            }
            if (!run && !shot.empty()) SDL_Log("screenshot %s: %s", shot.c_str(), saveShot(ren, shot.c_str()) ? "ok" : SDL_GetError());
            SDL_RenderPresent(ren);
            if (!headless) SDL_Delay(1);
        }
    }
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return rc;
}
