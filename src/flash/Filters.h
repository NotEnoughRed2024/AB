// CPU implementations of Flash bitmap filters + a small pool of reusable SDL textures.
#pragma once
#include <SDL3/SDL.h>

#include <cstdint>
#include <map>
#include <tuple>
#include <vector>

#include "Library.h"

namespace flash {

// premultiplied ARGB pixels
struct PixelBuffer {
    int w = 0, h = 0;
    std::vector<uint32_t> px;
};

// scale: pixels per Flash pixel (filter sizes are in stage pixels)
void applyFilterCPU(PixelBuffer& pb, const Filter& f, double scale);
void boxBlur(PixelBuffer& pb, double bx, double by, int passes);

class TexturePool {
public:
    static TexturePool& get();
    SDL_Texture* acquire(SDL_Renderer* r, int w, int h, SDL_TextureAccess access);
    void endFrame();  // everything acquired this frame becomes reusable
    void clear();

private:
    using Key = std::tuple<int, int, int>;
    std::map<Key, std::vector<SDL_Texture*>> free_;
    std::vector<std::pair<Key, SDL_Texture*>> used_;
};

}  // namespace flash
