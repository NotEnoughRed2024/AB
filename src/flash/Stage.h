// The Stage owns the root clip, drives the 40 fps frame loop, input, and renders the display list with SDL3.
#pragma once
#include <SDL3/SDL.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Display.h"
#include "json.hpp"

namespace flash {

struct Listener {
    std::function<void()> onMouseDown, onMouseUp, onMouseMove, onKeyDown, onKeyUp;
};

class Stage {
public:
    static Stage* get() { return instance; }
    Stage(SDL_Window* w, SDL_Renderer* r, int width, int height, double fps);
    ~Stage();

    SDL_Window* window;
    SDL_Renderer* renderer;
    int width, height;  // logical stage size (400x360)
    double fps;
    double viewScale = 2;  // window pixels per logical pixel
    bool customBlend = true;  // false on renderers without SDL_ComposeCustomBlendMode support (software)
    double viewX = 0, viewY = 0;
    // window-pixel rectangle the stage is drawn into (w <= 0: the whole window). The page around the
    // game (src/site) draws the rest of the window itself.
    SDL_FRect gameRect{0, 0, 0, 0};
    bool pointerInGame = true;   // pointer currently over the game rectangle
    bool gameOwnsButton = false; // the current mouse press started inside the game
    void updateView();
    bool insideGame(float wx, float wy) const;
    void applyCursor();
    MC root;
    std::unordered_map<std::string, std::unique_ptr<Library>> libraries;
    Library* mainLib = nullptr;

    Library* loadLibrary(const std::string& dir, const std::string& name);

    // input
    double mouseX = 0, mouseY = 0;
    bool mouseDown = false;
    bool keys[256] = {};
    int lastKey = 0;
    std::vector<std::shared_ptr<Listener>> mouseListeners, keyListeners;
    bool mouseHidden = false;
    bool isDown(int flashKeyCode) const { return flashKeyCode >= 0 && flashKeyCode < 256 && keys[flashKeyCode]; }
    void handleEvent(const SDL_Event& e);

    // frame
    void tick();  // advance timelines, run actions, onEnterFrame
    void render(bool clear = true);
    void queueFrameActions(const MC& mc);
    void runActionQueue();
    double timeMs() const;

    // rendering helpers shared with BitmapData
    void renderObject(DisplayObject* o, const Matrix& m, const ColorTransform& ct, Blend blend, bool premulTarget);
    void drawImage(Library* lib, int image, const Matrix& m, const ColorTransform& ct, Blend blend);
    void drawTexture(SDL_Texture* t, double srcW, double srcH, const Matrix& m, const ColorTransform& ct, Blend blend,
                     bool premultiplied, const SDL_FRect* srcRect = nullptr);
    SDL_BlendMode sdlBlend(Blend b, bool premultiplied) const;
    // view matrix currently in effect (stage logical -> current target pixels)
    Matrix view;
    SDL_Texture* currentTarget = nullptr;
    int targetW = 0, targetH = 0;

    // persistence (SharedObject)
    nlohmann::json& sharedObject(const std::string& name);
    void flushSharedObject(const std::string& name);

    // pointer interaction
    void updatePointer();
    std::weak_ptr<MovieClip> hover, pressed;

private:
    static Stage* instance;
    std::vector<std::weak_ptr<MovieClip>> actionQueue;
    std::unordered_map<std::string, nlohmann::json> shared;
    void renderFiltered(DisplayObject* o, const Matrix& m, const ColorTransform& ct, Blend blend, bool premulTarget);
    void renderMasked(MovieClip* parent, std::map<int, std::shared_ptr<DisplayObject>>::iterator maskIt,
                      const Matrix& m, const ColorTransform& ct, Blend blend, bool premulTarget,
                      std::map<int, std::shared_ptr<DisplayObject>>::iterator& resumeIt);
    void renderText(TextField* tf, const Matrix& m, const ColorTransform& ct, Blend blend);
    void drawStrokes(const std::vector<MovieClip::Stroke>& strokes, const Matrix& m, const ColorTransform& ct, Blend blend);
    void collectEnterFrame(MovieClip* mc, std::vector<MC>& out);
    MC findInteractive(MovieClip* mc, double gx, double gy);
    Uint64 startTicks;
};

// Flash key codes used by the game
namespace Key {
enum { BACKSPACE = 8, TAB = 9, ENTER = 13, SHIFT = 16, CONTROL = 17, ESCAPE = 27, SPACE = 32, LEFT = 37, UP = 38, RIGHT = 39, DOWN = 40 };
}

}  // namespace flash
