// el.*, ev.*, fx.* from the original client.
#pragma once
#include "Game.h"

namespace el {

class Ball : public Element {
public:
    static constexpr double ANGLE_MAX = 1.2;
    explicit Ball(MC mc);
    bool flReady = true, flPierce = false, flUp = true, flImmortal = false, flBounce = false;
    int type = -1;
    int fam = 0;
    double pdec = hx::NUL, speed = 5, angle = 0, damage = 1, baseDamage = 1, pierceDamage = 0;
    double gluePoint = hx::NUL;
    double ray = 4, va = 0, ca = 0;
    double sleep = hx::NUL;
    Block* trg = nullptr;

    void update() override;
    void colPad(double cx);
    void setGhost(bool fl) override;
    void setType(int n);
    void setSpeed(double n);
    void setAngle(double a);
    void unglue();
    Ball* clone();
    void fxLight();
    void fxPowerUp();
    void destroy();
    void kill() override;

    int hType() const override { return type; }
    int hFam() const override { return fam; }
    double hDamage() const override { return damage; }
    void hDestroy() override { destroy(); }

protected:
    void onBounce(int px, int py) override;
    void onEnterSquare(int sx, int sy) override;

private:
    void initFamily();
    void checkPad();
    void genSparks(int fr, double turn);
    void genIceShards();
    void genBubbles();
};

class Molecule : public Element {
public:
    static int SUM;
    struct Info {
        int pb;
        int mols[3];
    };
    static const Info INFOS[7];
    static constexpr int SPEEDER = 0, BUILDER = 1, CLEANER = 2, SPACER = 3, ARMORER = 4, FIXER = 5, LIGHTER = 6;

    explicit Molecule(MC mc);
    double ray = 12;
    void setType(int n);
    void update() override;
    void damage(Ball* ball);
    void setSpeed(double n);
    void setAngle(double a);
    void explode();
    void kill() override;
    static int getRandomMolType(mt::OldRandom& seed);

protected:
    void onEnterSquare(int sx, int sy) override;
    void onBounce(int px, int py) override;

private:
    struct Mol {
        Sprite* sp;
        double tx = 0, ty = 0;
    };
    bool flDeath = false;
    int type = -1;
    double decal = 0, speed = 1;
    int yLim = 0;
    std::vector<Mol> list;
    std::unique_ptr<mt::DepthManager> mdm;
    void newTrg(Mol& m);
    void insertInGrid(int x, int y);
    void removeFromGrid(int x, int y);
};

class Shot : public Element {
public:
    explicit Shot(MC mc);
    double damage = 1;
    double hDamage() const override { return damage; }
    virtual void hit();

protected:
    void onEnterSquare(int sx, int sy) override;
    void onBounce(int px, int py) override;
};

namespace shot {
class Laser : public Shot {
public:
    explicit Laser(MC mc);
    void update() override;
    void setVit(double n);
    void hit() override;

private:
    bool flHit = false;
    double height = 44, vit = 0;
};

class Missile : public Shot {
public:
    explicit Missile(MC mc);
    int splash = -1;
    void setType(int n);
    void update() override;

protected:
    void onBounce(int px, int py) override;
};
}  // namespace shot

}  // namespace el

namespace ev {

class Generator : public Event {
public:
    explicit Generator(Block* bl);
    Block* bl;
    void update() override;

private:
    double timer;
};

class Indigestion : public Event {
public:
    Indigestion();
};

class Javelot : public Event {
public:
    Javelot();
    void update() override;
    void kill() override;

private:
    MC mcJavelot;
    int x, y;
};

class Ouverture : public Event {
public:
    Ouverture();
    void update() override;
    void crush();

private:
    double timer = 0;
    int range = 3;
};

class Quasar : public Event {
public:
    static constexpr int RAY = 100;
    Quasar();
    void update() override;
    void kill() override;

private:
    struct PartBlock {
        Phys* p;
        double a, c, cs, speed, acc;
        bool flBlock;
    };
    MC mcCenter;
    std::vector<PartBlock> list;
    double timer = 60;
    PartBlock& newPart(MC mc, double x, double y);
};

class Dragon : public Event {
public:
    Dragon(Block* bl, int sens);
    void update() override;
    void kill() override;

private:
    MC mcDragon;
    int x, y, sens;
};

class Storm : public Event {
public:
    static int TOTAL;
    Storm();
    Block* bl = nullptr;
    void update() override;
    void kill() override;

private:
    double timer = 40;
    MC root;
};

class UltraViolet : public Event {
public:
    UltraViolet();
    void update() override;

private:
    double timer = 0;
    int ammo = 30;
    void shoot();
};

class Unification : public Event {
public:
    Unification();
    void update() override;

private:
    double cx, cy, ray = 0;
    std::vector<std::pair<Block*, double>> list;
};

class Wave : public Event {
public:
    Wave();
    void update() override;
    void kill() override;

private:
    MC mcWave;
    int y;
};

class Drone : public Event {
public:
    Drone();
    MC root;
    double sparkTimer = hx::NUL;
    Block* bl = nullptr;
    void update() override;
    void findTrg();
    static Block* getBlock();
    static Block* getRandomBonusBlock();
    static Block* getRandomSentiBlock();
    void explode();
    void kill() override;

private:
    double transformSpeed = 0.0022, jumpSpeed = 6;
    bool flConverter = false;
    int step = -1;
    double c = 0, sx = 0, sy = 0, ex = 0, ey = 0, speed = 0, frame = 0;
    void followPath();
    void setTrg(double x, double y);
    void updateJump();
    void initBlock();
    void updateBlock();
    void findBlockTrg();
    void initCancel();
    void updateCancel();
};

}  // namespace ev

namespace fx {

class Attract : public Phys {
public:
    explicit Attract(MC mc) : Phys(std::move(mc)) {}
    double dx = 0, dy = 0;
    void update() override;
};

class Fly : public Phys {
public:
    Fly();
    void update() override;

private:
    int step = 0;
    double dx = hx::NUL, dy = 0, ox = 0, life = 0, angle = 0, speed = 0;
};

class Jumper : public Phys {
public:
    explicit Jumper(MC mc);
    Block* bl = nullptr;
    void update() override;
    void seekTrg();

private:
    int tx = -1, ty = 0;
    double sx = 0, sy = 0, ex = 0, ey = 0, c = 0;
};

class LineUp : public Phys {
public:
    explicit LineUp(MC mc) : Phys(std::move(mc)) {}
    double factor = 1;
    void update() override;
};

class Part : public Phys {
public:
    explicit Part(MC mc);
    std::unique_ptr<Bouncer> bouncer;
    void update() override;
};

class Spark : public Phys {
public:
    explicit Spark(MC mc) : Phys(std::move(mc)) {}
    double coef = 1;
    void update() override;
};

class Tracer : public Phys {
public:
    explicit Tracer(MC mc) : Phys(std::move(mc)) {}
    void update() override;
};

}  // namespace fx
