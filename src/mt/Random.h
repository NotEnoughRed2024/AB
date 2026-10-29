// Seeded generators used by the original game. Both reproduce Flash 8 number semantics exactly,
// so seeded content (levels, options, palettes) matches the original.
#pragma once
#include <SDL3/SDL.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace mt {

// ECMAScript ToInt32 applied to an already-truncated double (what AVM1's bitwise ops do)
inline int32_t toInt32(double d) {
    if (!std::isfinite(d)) return 0;
    double t = std::trunc(d);
    double m = std::fmod(t, 4294967296.0);
    if (m < 0) m += 4294967296.0;
    return (int32_t)(uint32_t)m;
}

// mt.OldRandom: lagged generator from OCaml's stdlib, 30-bit values.
class OldRandom {
public:
    explicit OldRandom(int seed = 0, int prec = 55) : prec(prec), idx(0) {
        vals.reserve(prec);
        int s = seed;
        for (int i = 0; i < prec; i++) {
            // Std.int(seed * 1103515245.0) is a double multiply (rounded to 53 bits) then truncation;
            // the following "& 0x3FFFFFFF" applies ToInt32.
            s = toInt32(std::trunc((double)s * 1103515245.0)) & 0x3FFFFFFF;
            s += 12345;
            s &= 0x3FFFFFFF;
            int sbig = s & 0x3FFF0000;
            s = toInt32(std::trunc((double)s * 1103515245.0)) & 0x3FFFFFFF;
            s += 12345;
            s &= 0x3FFFFFFF;
            vals.push_back((s >> 16) | sbig);
        }
    }
    int next() {
        idx = (idx + 1) % prec;
        int v = (vals[(idx + 24) % prec] + idx) & 0x3FFFFFFF;
        vals[idx] = v;
        return v;
    }
    int random(int max) {
        int v = next();
        if (max <= 0) {
            SDL_Log("OldRandom.random(%d): original returns NaN here", max);
            return 0;
        }
        return v % max;
    }
    double rand() { return (next() % 1000) / 1000.0; }

private:
    int prec, idx;
    std::vector<int> vals;
};

// The game's own Random.hx (an LCG); 1664525*seed fits in a double exactly, so int64 matches.
class Random {
public:
    explicit Random(int s) : seed(s) {}
    double rand() {
        step();
        return (seed % 1000) / 1000.0;
    }
    int random(int n) {
        step();
        return n > 0 ? seed % n : 0;
    }

private:
    int seed;
    void step() {
        double p = 1664525.0 * seed + 1013904223.0;
        seed = toInt32(std::trunc(p)) & 0x3FFFFFFF;  // Std.int rounds toward zero
    }
};

// mt.Rand (Parker-Miller-Carta LCG), Flash 8 build: Int arithmetic is done on doubles there.
class Rand {
public:
    explicit Rand(int s) : seed((double)(s < 0 ? -(double)s : s) + 131) {}
    int random(int n) { return n > 0 ? next() % n : 0; }
    double rand() { return (next() % 10007) / 10007.0; }

private:
    double seed;
    int next() {
        seed = std::fmod(seed * 16807.0, 2147483647.0);
        return toInt32(seed) & 0x3FFFFFFF;
    }
};

}  // namespace mt
