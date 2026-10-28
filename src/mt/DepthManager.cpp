#include "DepthManager.h"

#include <cmath>

namespace mt {

int DepthManager::instCounter = 0;

DepthManager::Plan& DepthManager::getPlan(int n) {
    if (n >= (int)plans.size()) plans.resize(n + 1);
    return plans[n];
}

void DepthManager::compact(int plan) {
    Plan& pd = plans[plan];
    int cur = 0;
    int base = plan * 1000;
    for (int i = 0; i < pd.cur; i++) {
        auto& o = pd.tbl[i];
        if (o && !o->removed) {
            o->swapDepths(base + cur);
            pd.tbl[cur] = o;
            cur++;
        }
    }
    pd.tbl.resize(cur);
    pd.cur = cur;
}

flash::MC DepthManager::attach(const std::string& inst, int plan) {
    Plan& pd = getPlan(plan);
    if (pd.cur == 1000) {
        compact(plan);
        return attach(inst, plan);
    }
    int d = pd.cur;
    auto mc = root->attachMovie(inst, inst + "@" + std::to_string(instCounter++), d + plan * 1000);
    if ((int)pd.tbl.size() <= d) pd.tbl.resize(d + 1);
    pd.tbl[d] = mc;
    pd.cur = d + 1;
    return mc;
}

flash::MC DepthManager::empty(int plan) {
    Plan& pd = getPlan(plan);
    if (pd.cur == 1000) {
        compact(plan);
        return empty(plan);
    }
    int d = pd.cur;
    auto mc = root->createEmptyMovieClip("empty@" + std::to_string(instCounter++), d + plan * 1000);
    if ((int)pd.tbl.size() <= d) pd.tbl.resize(d + 1);
    pd.tbl[d] = mc;
    pd.cur = d + 1;
    return mc;
}

void DepthManager::attachBitmap(flash::BMP bmp, int plan) {
    Plan& pd = getPlan(plan);
    if (pd.cur == 1000) {
        compact(plan);
        attachBitmap(bmp, plan);
        return;
    }
    int d = pd.cur;
    root->attachBitmap(bmp, d + plan * 1000);
    if ((int)pd.tbl.size() <= d) pd.tbl.resize(d + 1);
    pd.tbl[d] = nullptr;
    pd.cur = d + 1;
}

int DepthManager::reserve(const std::shared_ptr<flash::DisplayObject>& mc, int plan) {
    Plan& pd = getPlan(plan);
    if (pd.cur == 1000) {
        compact(plan);
        return reserve(mc, plan);
    }
    int d = pd.cur;
    if ((int)pd.tbl.size() <= d) pd.tbl.resize(d + 1);
    pd.tbl[d] = mc;
    pd.cur = d + 1;
    return d + plan * 1000;
}

void DepthManager::swap(const flash::MC& mc, int plan) {
    int src = (int)std::floor(mc->getDepth() / 1000.0);
    if (src == plan) return;
    Plan& pd = getPlan(src);
    for (int i = 0; i < pd.cur; i++)
        if (pd.tbl[i] == mc) {
            pd.tbl[i] = nullptr;
            break;
        }
    mc->swapDepths(reserve(mc, plan));
}

void DepthManager::under(const flash::MC& mc) {
    int d = mc->getDepth();
    int plan = (int)std::floor(d / 1000.0);
    Plan& pd = getPlan(plan);
    int pdd = d % 1000;
    if (pdd < (int)pd.tbl.size() && pd.tbl[pdd] == mc) {
        pd.tbl[pdd] = nullptr;
        pd.tbl.insert(pd.tbl.begin(), mc);
        pd.cur++;
        compact(plan);
    }
}

void DepthManager::over(const flash::MC& mc) {
    int d = mc->getDepth();
    int plan = (int)std::floor(d / 1000.0);
    Plan& pd = getPlan(plan);
    int pdd = d % 1000;
    if (pdd < (int)pd.tbl.size() && pd.tbl[pdd] == mc) {
        pd.tbl[pdd] = nullptr;
        if (pd.cur == 1000) compact(plan);
        int nd = pd.cur;
        pd.cur++;
        mc->swapDepths(nd + plan * 1000);
        if ((int)pd.tbl.size() <= nd) pd.tbl.resize(nd + 1);
        pd.tbl[nd] = mc;
    }
}

void DepthManager::clear(int plan) {
    Plan& pd = getPlan(plan);
    for (int i = 0; i < pd.cur && i < (int)pd.tbl.size(); i++)
        if (pd.tbl[i]) pd.tbl[i]->removeMovieClip();
    // bitmaps attached through attachBitmap aren't tracked; remove anything left in the plan's range
    for (int dd = plan * 1000; dd < plan * 1000 + pd.cur; dd++) root->removeChildAt(dd);
    pd.tbl.clear();
    pd.cur = 0;
}

void DepthManager::ysort(int plan) {
    Plan& pd = getPlan(plan);
    double y = -99999999;
    for (int i = 0; i < pd.cur; i++) {
        auto mc = pd.tbl[i];
        if (!mc) continue;
        double mcy = mc->y;
        if (mcy >= y) {
            y = mcy;
        } else {
            int j = i;
            while (j > 0) {
                auto mc2 = pd.tbl[j - 1];
                if (mc2 && mc2->y > mcy) {
                    pd.tbl[j] = mc2;
                    mc->swapDepths(mc2->getDepth());
                } else {
                    pd.tbl[j] = mc;
                    break;
                }
                j--;
            }
            if (j == 0) pd.tbl[0] = mc;
        }
    }
}

void DepthManager::destroy() {
    for (int i = 0; i < (int)plans.size(); i++) clear(i);
}

}  // namespace mt
