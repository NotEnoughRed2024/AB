// Geometry helpers mirroring flash.geom (Matrix, ColorTransform, Rectangle, Point).
#pragma once
#include <algorithm>
#include <cmath>

namespace flash {

struct Point {
    double x = 0, y = 0;
};

struct Rect {
    double xMin = 0, yMin = 0, xMax = 0, yMax = 0;
    bool empty = true;
    void add(double x, double y) {
        if (empty) {
            xMin = xMax = x;
            yMin = yMax = y;
            empty = false;
        } else {
            xMin = std::min(xMin, x);
            xMax = std::max(xMax, x);
            yMin = std::min(yMin, y);
            yMax = std::max(yMax, y);
        }
    }
    void add(const Rect& r) {
        if (r.empty) return;
        add(r.xMin, r.yMin);
        add(r.xMax, r.yMax);
    }
    double width() const { return empty ? 0 : xMax - xMin; }
    double height() const { return empty ? 0 : yMax - yMin; }
};

// x' = a*x + c*y + tx ; y' = b*x + d*y + ty   (same layout as flash.geom.Matrix)
struct Matrix {
    double a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;

    static Matrix identity() { return Matrix{}; }
    static Matrix make(double a, double b, double c, double d, double tx, double ty) {
        Matrix m;
        m.a = a; m.b = b; m.c = c; m.d = d; m.tx = tx; m.ty = ty;
        return m;
    }
    // result = this ∘ other  (apply other first, then this)
    Matrix operator*(const Matrix& o) const {
        Matrix r;
        r.a = a * o.a + c * o.b;
        r.b = b * o.a + d * o.b;
        r.c = a * o.c + c * o.d;
        r.d = b * o.c + d * o.d;
        r.tx = a * o.tx + c * o.ty + tx;
        r.ty = b * o.tx + d * o.ty + ty;
        return r;
    }
    Point apply(double x, double y) const { return {a * x + c * y + tx, b * x + d * y + ty}; }
    Matrix inverted() const {
        double det = a * d - b * c;
        Matrix r;
        if (std::fabs(det) < 1e-12) {
            r.a = r.d = 0;
            r.tx = -tx;
            r.ty = -ty;
            return r;
        }
        r.a = d / det;
        r.b = -b / det;
        r.c = -c / det;
        r.d = a / det;
        r.tx = -(r.a * tx + r.c * ty);
        r.ty = -(r.b * tx + r.d * ty);
        return r;
    }
    // flash.geom.Matrix mutators (they post-multiply: the new op is applied after the existing transform)
    void scale(double sx, double sy) {
        a *= sx; c *= sx; tx *= sx;
        b *= sy; d *= sy; ty *= sy;
    }
    void rotate(double ang) {
        double cs = std::cos(ang), sn = std::sin(ang);
        Matrix r = make(cs, sn, -sn, cs, 0, 0);
        *this = r * (*this);
    }
    void translate(double x, double y) {
        tx += x;
        ty += y;
    }
    Rect transformRect(const Rect& r) const {
        Rect out;
        if (r.empty) return out;
        Point p;
        p = apply(r.xMin, r.yMin); out.add(p.x, p.y);
        p = apply(r.xMax, r.yMin); out.add(p.x, p.y);
        p = apply(r.xMin, r.yMax); out.add(p.x, p.y);
        p = apply(r.xMax, r.yMax); out.add(p.x, p.y);
        return out;
    }
};

// rgba' = rgba * mult + add   (add in 0..255 units, like flash.geom.ColorTransform)
struct ColorTransform {
    double rm = 1, gm = 1, bm = 1, am = 1;
    double ra = 0, ga = 0, ba = 0, aa = 0;

    ColorTransform() = default;
    ColorTransform(double rm_, double gm_, double bm_, double am_, double ra_ = 0, double ga_ = 0, double ba_ = 0,
                   double aa_ = 0)
        : rm(rm_), gm(gm_), bm(bm_), am(am_), ra(ra_), ga(ga_), ba(ba_), aa(aa_) {}

    bool isIdentity() const {
        return rm == 1 && gm == 1 && bm == 1 && am == 1 && ra == 0 && ga == 0 && ba == 0 && aa == 0;
    }
    bool hasAdd() const { return ra != 0 || ga != 0 || ba != 0; }
    // result = parent ∘ this (apply this first, then parent)
    ColorTransform concat(const ColorTransform& parent) const {
        ColorTransform r;
        r.rm = rm * parent.rm;
        r.gm = gm * parent.gm;
        r.bm = bm * parent.bm;
        r.am = am * parent.am;
        r.ra = ra * parent.rm + parent.ra;
        r.ga = ga * parent.gm + parent.ga;
        r.ba = ba * parent.bm + parent.ba;
        r.aa = aa * parent.am + parent.aa;
        return r;
    }
    void applyTo(double& r, double& g, double& b, double& a) const {
        r = r * rm + ra;
        g = g * gm + ga;
        b = b * bm + ba;
        a = a * am + aa;
    }
};

}  // namespace flash
