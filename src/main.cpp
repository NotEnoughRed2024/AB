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
//   --tab N               open the page on tab N (0 missions, 1 my page, 2 records, 3 help, 4 universe map, 5 debug)
//   --size WxH            window size in pixels (testing)
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "flash/Filters.h"
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

    if (const char* seed = SDL_getenv("AB_SEED")) SDL_srand((Uint64)std::atoll(seed));  // repeatable test runs
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
        // test hook: AB_TEST_CLICKS="frame:x,y;..." clicks at stage coordinates on the given frames
        struct TestClick {
            int frame;
            double x, y;
            bool window = false;  // "w" prefix: window pixels instead of stage coordinates
            int hold = 2;         // "+N" suffix: frames the button stays down
        };
        std::vector<TestClick> clicks;
        if (const char* tc = SDL_getenv("AB_TEST_CLICKS")) {
            std::string all = tc;
            size_t p = 0;
            while (p < all.size()) {
                size_t e = all.find(';', p);
                std::string item = all.substr(p, e == std::string::npos ? std::string::npos : e - p);
                TestClick c{};
                if (!item.empty() && item[0] == 'w') {
                    c.window = true;
                    item = item.substr(1);
                }
                if (std::sscanf(item.c_str(), "%d:%lf,%lf+%d", &c.frame, &c.x, &c.y, &c.hold) >= 3) clicks.push_back(c);
                if (e == std::string::npos) break;
                p = e + 1;
            }
        }
        auto injectClicks = [&](int frame) {
            for (auto& c : clicks) {
                float wx = (float)(stage.viewX + c.x * stage.viewScale), wy = (float)(stage.viewY + c.y * stage.viewScale);
                if (c.window) {
                    wx = (float)c.x;
                    wy = (float)c.y;
                }
                SDL_Event ev{};
                if (c.frame == frame) {
                    ev.type = SDL_EVENT_MOUSE_MOTION;
                    ev.motion.x = wx;
                    ev.motion.y = wy;
                    SDL_PushEvent(&ev);
                    ev = SDL_Event{};
                    ev.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                    ev.button.button = SDL_BUTTON_LEFT;
                    ev.button.x = wx;
                    ev.button.y = wy;
                    SDL_PushEvent(&ev);
                } else if (c.frame + c.hold == frame) {
                    ev.type = SDL_EVENT_MOUSE_BUTTON_UP;
                    ev.button.button = SDL_BUTTON_LEFT;
                    ev.button.x = wx;
                    ev.button.y = wy;
                    SDL_PushEvent(&ev);
                }
            }
        };
        // test hook: AB_TEST_KEYS="frame:Key;..." presses keys (SDL key names, "S+" prefix = with Shift)
        std::vector<std::pair<int, std::string>> testKeys;
        if (const char* tk = SDL_getenv("AB_TEST_KEYS")) {
            std::string all = tk;
            size_t p = 0;
            while (p < all.size()) {
                size_t e = all.find(';', p);
                std::string item = all.substr(p, e == std::string::npos ? std::string::npos : e - p);
                size_t c = item.find(':');
                if (c != std::string::npos) testKeys.push_back({std::atoi(item.c_str()), item.substr(c + 1)});
                if (e == std::string::npos) break;
                p = e + 1;
            }
        }
        long long loops = 0;
        stage.loadDebugPrefs();
        Uint64 fpsStart = SDL_GetTicksNS(), updateNs = 0, drawNs = 0;
        int drawnFrames = 0, ticksDone = 0;
        while (run) {
            if (!clicks.empty()) injectClicks(count);
            loops++;
            for (auto& k : testKeys)
                if (k.first == loops) {  // by loop, not tick, so keys still arrive while paused
                    std::string name = k.second;
                    bool sh = name.rfind("S+", 0) == 0;
                    if (sh) name = name.substr(2);
                    SDL_Event ev{};
                    ev.type = SDL_EVENT_KEY_DOWN;
                    ev.key.key = SDL_GetKeyFromName(name.c_str());
                    ev.key.mod = sh ? SDL_KMOD_LSHIFT : SDL_KMOD_NONE;
                    ev.key.down = true;
                    SDL_PushEvent(&ev);
                }
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) run = false;
                if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_F3) {
                    stage.debug.showFps = !stage.debug.showFps;
                    stage.saveDebugPrefs();
                    continue;
                }
                // debug keys: F4 pause, F5 one frame at a time, 1-0 (Shift: more) drop capsules at the pointer
                if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F4 && !e.key.repeat) {
                    stage.debug.paused = !stage.debug.paused;
                    stage.debug.stepFrames = 0;
                    continue;
                }
                if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F5) {  // held: keeps stepping
                    stage.debug.paused = true;
                    stage.debug.stepFrames++;
                    continue;
                }
                if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key >= SDLK_0 && e.key.key <= SDLK_9 &&
                    !(e.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI))) {
                    int slot = e.key.key == SDLK_0 ? 9 : (int)(e.key.key - SDLK_1);
                    shell.debugDrop(slot, (e.key.mod & SDL_KMOD_SHIFT) != 0);
                    continue;
                }
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
            Uint64 t0 = SDL_GetTicksNS();
            if (stage.debug.paused) {
                // frozen: only F5 steps run (one tick each), at most 4 per drawn frame
                acc = 0;
                if (headless && stage.debug.stepFrames == 0 && frames > 0 && ++count >= frames) run = false;
                while (stage.debug.stepFrames > 0 && steps < 4) {
                    stage.tick();
                    stage.debug.stepFrames--;
                    stage.debug.frame++;
                    steps++;
                    count++;
                    if (frames > 0 && count >= frames) run = false;
                }
            }
            while (!stage.debug.paused && acc >= frameMs && steps < 4) {
                stage.debug.frame++;
                stage.tick();
                acc -= frameMs;
                steps++;
                count++;
                if (frames > 0 && count >= frames) run = false;
            }
            if (acc > frameMs * 4) acc = 0;
            Uint64 t1 = SDL_GetTicksNS();
            if (steps > 0) updateNs += (t1 - t0) / steps;
            ticksDone += steps;
            if (shell.quitRequested) run = false;
            if (SDL_getenv("AB_STATS") && count % 40 == 0)
                SDL_Log("frame %d: filtered %d masked %d readback %d px, pool %zu textures %.1f MB", count,
                        stage.lastStats.filtered, stage.lastStats.masked, stage.lastStats.readbackPx,
                        flash::TexturePool::get().count(), flash::TexturePool::get().bytes() / 1048576.0);
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
            Uint64 t2 = SDL_GetTicksNS();
            drawNs += t2 - t1;
            drawnFrames++;
            if (t2 - fpsStart >= 500000000ULL) {
                // averaged over half a second
                double secs = (t2 - fpsStart) / 1e9;
                stage.debug.fps = drawnFrames / secs;
                stage.debug.tps = ticksDone / secs;
                ticksDone = 0;
                stage.debug.updateMs = updateNs / 1e6 / drawnFrames;
                stage.debug.drawMs = drawNs / 1e6 / drawnFrames;
                drawnFrames = 0;
                updateNs = drawNs = 0;
                fpsStart = t2;
            }
            stage.drawDebugOverlay();
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
