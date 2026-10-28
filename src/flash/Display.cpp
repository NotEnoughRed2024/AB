#include "Display.h"

#include <cmath>
#include <cstdlib>

#include "Stage.h"

namespace flash {

static constexpr double DEG = 3.14159265358979323846 / 180.0;

// ------------------------------------------------------------------ DisplayObject

Matrix DisplayObject::matrix() const {
    double sx = xscale / 100.0, sy = yscale / 100.0;
    double r = rotation * DEG, k = (rotation + skew) * DEG;
    return Matrix::make(sx * std::cos(r), sx * std::sin(r), -sy * std::sin(k), sy * std::cos(k), x, y);
}

void DisplayObject::setMatrix(const Matrix& m) {
    x = m.tx;
    y = m.ty;
    double sx = std::sqrt(m.a * m.a + m.b * m.b);
    double sy = std::sqrt(m.c * m.c + m.d * m.d);
    double det = m.a * m.d - m.b * m.c;
    double rot = std::atan2(m.b, m.a);
    double rot2 = std::atan2(-m.c, m.d);
    if (det < 0) {
        // mirrored: keep x positive, flip y (matches how Flash reports negative _yscale)
        sy = -sy;
        rot2 = std::atan2(m.c, -m.d);
    }
    xscale = sx * 100.0;
    yscale = sy * 100.0;
    rotation = rot / DEG;
    skew = (rot2 - rot) / DEG;
    while (skew > 180) skew -= 360;
    while (skew < -180) skew += 360;
    if (std::fabs(skew) < 1e-6) skew = 0;
}

void DisplayObject::setRotation(double v) {
    v = std::fmod(v, 360.0);
    if (v > 180) v -= 360;
    if (v < -180) v += 360;
    rotation = v;
    scripted = true;
}

ColorTransform DisplayObject::colorTransform() const {
    ColorTransform c = color;
    c.am *= alpha / 100.0;
    return c;
}

Matrix DisplayObject::concatenatedMatrix() const {
    Matrix m = matrix();
    for (const MovieClip* p = parent; p; p = p->parent) m = p->matrix() * m;
    return m;
}

Rect DisplayObject::getBounds(const DisplayObject* target) const {
    Rect lb = localBounds();
    if (lb.empty) {
        Rect r;
        Point p = localToGlobal({0, 0});
        if (target) p = target->globalToLocal(p);
        r.add(p.x, p.y);
        return r;
    }
    Matrix m = concatenatedMatrix();
    if (target) m = target->concatenatedMatrix().inverted() * m;
    return m.transformRect(lb);
}

double DisplayObject::width() const {
    Rect r = matrix().transformRect(localBounds());
    return r.width();
}
double DisplayObject::height() const {
    Rect r = matrix().transformRect(localBounds());
    return r.height();
}
void DisplayObject::setWidth(double w) {
    Rect lb = localBounds();
    if (lb.width() > 0) xscale = w / lb.width() * 100.0;
    scripted = true;
}
void DisplayObject::setHeight(double h) {
    Rect lb = localBounds();
    if (lb.height() > 0) yscale = h / lb.height() * 100.0;
    scripted = true;
}

Point DisplayObject::localToGlobal(Point p) const { return concatenatedMatrix().apply(p.x, p.y); }
Point DisplayObject::globalToLocal(Point p) const { return concatenatedMatrix().inverted().apply(p.x, p.y); }

void DisplayObject::removeMovieClip() {
    if (removed) return;
    removed = true;
    if (parent) {
        auto keep = shared_from_this();  // stay alive until we're done
        MovieClip* p = parent;
        auto it = p->children.find(depth);
        if (it != p->children.end() && it->second.get() == this) p->children.erase(it);
        parent = nullptr;
    }
}

void DisplayObject::swapDepths(int newDepth) {
    if (!parent || newDepth == depth) return;
    MovieClip* p = parent;
    auto self = shared_from_this();
    auto it = p->children.find(newDepth);
    std::shared_ptr<DisplayObject> other;
    if (it != p->children.end()) other = it->second;
    p->children.erase(depth);
    if (other) {
        other->depth = depth;
        p->children[depth] = other;
    }
    depth = newDepth;
    p->children[newDepth] = self;
}

Rect BitmapHolder::localBounds() const {
    Rect r;
    r.add(0, 0);
    r.add(bmp->width, bmp->height);
    return r;
}

// ------------------------------------------------------------------ MovieClip

MovieClip::MovieClip(Library* l, const Character* d) : DisplayObject(KClip), lib(l), def(d) {}
MovieClip::~MovieClip() = default;

std::shared_ptr<DisplayObject> MovieClip::instantiate(const Placement& p) {
    const Character* c = lib->character(p.character);
    if (!c) return nullptr;
    std::shared_ptr<DisplayObject> obj;
    switch (c->kind) {
        case Character::Shape:
        case Character::Bitmap:
            obj = std::make_shared<Shape>(lib, c->image);
            break;
        case Character::Sprite: {
            auto mc = std::make_shared<MovieClip>(lib, c);
            obj = mc;
            break;
        }
        case Character::EditText:
            obj = std::make_shared<TextField>(lib, c->edit);
            break;
        default:
            return nullptr;
    }
    obj->timelineInstance = p.instance;
    obj->name = p.name;
    obj->clipDepth = p.clipDepth;
    return obj;
}

static void applyPlacement(DisplayObject* o, const Placement& p) {
    if (!o->scripted) o->setMatrix(p.matrix);
    if (p.hasColor) {
        ColorTransform c = p.color;
        if (!o->scripted) {
            o->alpha = c.am * 100.0;
            c.am = 1;
        }
        o->color = c;
    } else if (!o->scripted) {
        o->color = ColorTransform();
        o->alpha = 100;
    }
    o->filters = p.filters;
    o->blendMode = p.blend;
    o->clipDepth = p.clipDepth;
}

void MovieClip::applyFrame(int f) {
    if (!def || def->frames.empty()) return;
    const Frame& fr = def->frames[f - 1];
    // timeline objects live at depth - 16384 (negative), like AS2
    std::map<int, const Placement*> want;
    for (const auto& p : fr.displayList) want[p.depth - 16384] = &p;
    // remove timeline objects that are gone or replaced
    for (auto it = children.begin(); it != children.end();) {
        auto& o = it->second;
        if (o->timelineInstance > 0) {
            auto w = want.find(it->first);
            if (w == want.end() || w->second->instance != o->timelineInstance) {
                o->removed = true;
                o->parent = nullptr;
                it = children.erase(it);
                continue;
            }
        }
        ++it;
    }
    for (auto& [d, p] : want) {
        auto it = children.find(d);
        if (it != children.end()) {
            if (it->second->timelineInstance == p->instance) {
                applyPlacement(it->second.get(), *p);
                continue;
            }
            continue;  // depth taken by a script object
        }
        auto obj = instantiate(*p);
        if (!obj) continue;
        applyPlacement(obj.get(), *p);
        obj->scripted = false;
        obj->parent = this;
        obj->depth = d;
        children[d] = obj;
        if (obj->kind == KClip) {
            auto* mc = static_cast<MovieClip*>(obj.get());
            mc->applyFrame(1);
            mc->constructed = true;
            Stage::get()->queueFrameActions(std::static_pointer_cast<MovieClip>(obj));
        }
    }
}

void MovieClip::gotoAndStop(int f) {
    playing = false;
    int n = totalFrames();
    if (f < 1) f = 1;
    if (f > n) f = n;
    if (f == frame && constructed) return;
    frame = f;
    applyFrame(f);
    Stage::get()->queueFrameActions(std::static_pointer_cast<MovieClip>(shared_from_this()));
}

void MovieClip::gotoAndStop(const std::string& label) {
    if (!def) return;
    auto it = def->labels.find(label);
    if (it != def->labels.end()) gotoAndStop(it->second);
}

void MovieClip::gotoAndPlay(int f) {
    gotoAndStop(f);
    playing = true;
}

void MovieClip::gotoAndPlay(const std::string& label) {
    gotoAndStop(label);
    playing = true;
}

void MovieClip::advance() {
    if (removed) return;
    // children first (copy: actions may remove clips)
    std::vector<std::shared_ptr<DisplayObject>> kids;
    kids.reserve(children.size());
    for (auto& [d, c] : children)
        if (c->kind == KClip) kids.push_back(c);
    for (auto& c : kids) static_cast<MovieClip*>(c.get())->advance();
    if (!playing || !def || def->frames.size() <= 1) return;
    int nf = frame + 1;
    if (nf > totalFrames()) nf = 1;
    frame = nf;
    applyFrame(nf);
    Stage::get()->queueFrameActions(std::static_pointer_cast<MovieClip>(shared_from_this()));
}

void MovieClip::runFrameActions() {
    if (removed || !def || def->frames.empty()) return;
    const Frame& fr = def->frames[frame - 1];
    for (auto& [a, arg] : fr.actions) {
        switch (a) {
            case FrameAction::Stop: playing = false; break;
            case FrameAction::Play: playing = true; break;
            case FrameAction::RandomFrame:
                if (vars.find("__a") == vars.end()) {  // the scripts guard with "if(a==null)"
                    vars["__a"] = 1;
                    gotoAndStop(1 + (int)(SDL_randf() * totalFrames()));
                }
                break;
            case FrameAction::RemoveSelf: removeMovieClip(); return;
            case FrameAction::CallObjKill:
                if (onObjKill) {
                    auto f = onObjKill;
                    f();
                } else {
                    removeMovieClip();
                }
                return;
            case FrameAction::RandomRotation: rotation = (double)(SDL_rand(360)); scripted = true; break;
            case FrameAction::CounterParentRotation:
                if (parent) { rotation = -parent->rotation; scripted = true; }
                break;
            case FrameAction::Rotate2: rotation += 2; scripted = true; break;
            case FrameAction::GotoAndPlay1: gotoAndPlay(1); break;
            case FrameAction::Goto: gotoAndStop(arg); break;
        }
    }
}

MC MovieClip::attachMovie(const std::string& linkage, const std::string& instName, int d) {
    int id = lib->exportId(linkage);
    const Character* c = id >= 0 ? lib->character(id) : nullptr;
    if (!c) {
        SDL_Log("attachMovie: unknown linkage '%s'", linkage.c_str());
        c = nullptr;
    }
    auto mc = std::make_shared<MovieClip>(lib, (c && c->kind == Character::Sprite) ? c : nullptr);
    mc->name = instName;
    if (c && c->kind != Character::Sprite) {
        // exported shape: wrap it
        Placement p;
        p.character = id;
        auto s = instantiate(p);
        if (s) {
            s->parent = mc.get();
            s->depth = -16383;
            s->timelineInstance = 0;
            mc->children[-16383] = s;
        }
    }
    addChild(mc, d);
    mc->applyFrame(1);
    mc->constructed = true;
    Stage::get()->queueFrameActions(mc);
    return mc;
}

MC MovieClip::createEmptyMovieClip(const std::string& instName, int d) {
    auto mc = std::make_shared<MovieClip>(lib, nullptr);
    mc->name = instName;
    mc->constructed = true;
    addChild(mc, d);
    return mc;
}

void MovieClip::attachBitmap(BMP bmp, int d) { addChild(std::make_shared<BitmapHolder>(std::move(bmp)), d); }

TF MovieClip::createTextField(const std::string& instName, int d, double x0, double y0, double w, double h) {
    EditTextDef e;
    e.bounds.empty = false;
    e.bounds.xMax = w;
    e.bounds.yMax = h;
    e.height = 12;
    auto tf = std::make_shared<TextField>(lib, e);
    tf->name = instName;
    tf->x = x0;
    tf->y = y0;
    addChild(tf, d);
    return tf;
}

void MovieClip::addChild(std::shared_ptr<DisplayObject> d, int dep) {
    auto it = children.find(dep);
    if (it != children.end()) {
        it->second->removed = true;
        it->second->parent = nullptr;
        children.erase(it);
    }
    d->parent = this;
    d->depth = dep;
    d->removed = false;
    children[dep] = std::move(d);
}

void MovieClip::removeChildAt(int dep) {
    auto it = children.find(dep);
    if (it == children.end()) return;
    it->second->removed = true;
    it->second->parent = nullptr;
    children.erase(it);
}

int MovieClip::nextHighestDepth() const {
    int d = 0;
    for (auto& [k, v] : children) d = std::max(d, k + 1);
    return d;
}

std::shared_ptr<DisplayObject> MovieClip::child(const std::string& n) const {
    for (auto& [d, c] : children)
        if (c->name == n) return c;
    return nullptr;
}

MC MovieClip::clip(const std::string& n) const {
    auto c = child(n);
    if (c && c->kind == KClip) return std::static_pointer_cast<MovieClip>(c);
    return nullptr;
}

TF MovieClip::field(const std::string& n) const {
    auto c = child(n);
    if (c && c->kind == KText) return std::static_pointer_cast<TextField>(c);
    return nullptr;
}

Rect MovieClip::localBounds() const {
    Rect r;
    for (auto& [d, c] : children) {
        if (!c->visible && c->kind != KClip) continue;
        Rect cb = c->localBounds();
        if (cb.empty) continue;
        r.add(c->matrix().transformRect(cb));
    }
    return r;
}

bool MovieClip::hitTest(double gx, double gy, bool shapeFlag) const {
    if (!shapeFlag) {
        Rect b = getBounds(nullptr);
        return !b.empty && gx >= b.xMin && gx <= b.xMax && gy >= b.yMin && gy <= b.yMax;
    }
    // shape test: any child's own bounds (good enough for the game's hit areas)
    for (auto& [d, c] : children) {
        if (!c->visible) continue;
        if (c->kind == KClip) {
            if (static_cast<const MovieClip*>(c.get())->hitTest(gx, gy, true)) return true;
        } else {
            Rect b = c->getBounds(nullptr);
            if (!b.empty && gx >= b.xMin && gx <= b.xMax && gy >= b.yMin && gy <= b.yMax) return true;
        }
    }
    return false;
}

bool MovieClip::hitTestClip(const MovieClip* other) const {
    Rect a = getBounds(nullptr), b = other->getBounds(nullptr);
    if (a.empty || b.empty) return false;
    return a.xMin <= b.xMax && b.xMin <= a.xMax && a.yMin <= b.yMax && b.yMin <= a.yMax;
}

Point MovieClip::mouse() const {
    Stage* s = Stage::get();
    return globalToLocal({s->mouseX, s->mouseY});
}

}  // namespace flash
