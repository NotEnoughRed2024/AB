// Small helpers reproducing Haxe 2 / Flash 8 (AVM1) semantics that the original code relies on.
#pragma once
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "flash/Stage.h"

namespace hx {

// Haxe "Float" that may be null: NaN plays the role of null.
inline const double NUL = std::numeric_limits<double>::quiet_NaN();
inline bool isNull(double v) { return std::isnan(v); }

// Std.int on Flash 8: floor for positives, ceil for negatives (no 32-bit wrap).
inline int Int(double x) {
    if (std::isnan(x)) return 0;
    return (int)(x < 0 ? std::ceil(x) : std::floor(x));
}
// Math.round in AVM1 = floor(x + 0.5)
inline double round(double x) { return std::floor(x + 0.5); }
// Math.random / Std.random
inline double random() { return SDL_randf(); }
inline int random(int n) { return n <= 0 ? 0 : (int)std::floor(SDL_randf() * n); }
// flash.Lib.getTimer()
inline double getTimer() { return flash::Stage::get() ? flash::Stage::get()->timeMs() : 0; }

template <class T>
bool remove(std::vector<T>& v, const T& x) {
    auto it = std::find(v.begin(), v.end(), x);
    if (it == v.end()) return false;
    v.erase(it);
    return true;
}
template <class T>
bool contains(const std::vector<T>& v, const T& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

// A grid cell that can be undefined (never set), null (explicitly cleared) or a number,
// reproducing how AVM1 compares/adds those (undefined -> NaN, null -> 0).
struct Cell {
    enum State : uint8_t { Undef, Null, Num } st = Undef;
    double v = 0;
    Cell() = default;
    Cell(double x) : st(Num), v(x) {}
    static Cell null() {
        Cell c;
        c.st = Null;
        return c;
    }
    bool empty() const { return st != Num; }   // "== null" in Haxe
    bool has() const { return st == Num; }
    double num() const { return st == Num ? v : (st == Null ? 0.0 : NUL); }  // ToNumber
    int i() const { return (int)v; }
    bool operator==(double x) const { return st == Num && v == x; }
    bool operator!=(double x) const { return !(*this == x); }
    bool lt(double x) const { double n = num(); return !std::isnan(n) && n < x; }
};

// 2D sparse array with Haxe/AVM1 out-of-range behaviour (reads give undefined, writes are ignored when negative).
struct Grid {
    std::vector<std::vector<Cell>> cols;
    Cell get(int x, int y) const {
        if (x < 0 || x >= (int)cols.size()) return Cell();
        const auto& c = cols[x];
        if (y < 0 || y >= (int)c.size()) return Cell();
        return c[y];
    }
    void set(int x, int y, Cell v) {
        if (x < 0 || y < 0) return;
        if (x >= (int)cols.size()) cols.resize(x + 1);
        auto& c = cols[x];
        if (y >= (int)c.size()) c.resize(y + 1);
        c[y] = v;
    }
    bool colExists(int x) const { return x >= 0 && x < (int)cols.size(); }
};

}  // namespace hx
