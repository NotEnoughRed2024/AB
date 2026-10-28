// mt.DepthManager (AS2 version): organises a clip's children into "plans" (layers) of 1000 depths.
#pragma once
#include <string>
#include <vector>

#include "flash/Display.h"

namespace mt {

class DepthManager {
public:
    explicit DepthManager(flash::MC root) : root(std::move(root)) {}
    flash::MC getMC() const { return root; }

    flash::MC attach(const std::string& inst, int plan);
    flash::MC empty(int plan);
    void attachBitmap(flash::BMP bmp, int plan);
    void swap(const flash::MC& mc, int plan);
    void under(const flash::MC& mc);
    void over(const flash::MC& mc);
    void clear(int plan);
    void ysort(int plan);
    void destroy();

private:
    struct Plan {
        std::vector<std::shared_ptr<flash::DisplayObject>> tbl;
        int cur = 0;
    };
    flash::MC root;
    std::vector<Plan> plans;
    static int instCounter;
    Plan& getPlan(int n);
    void compact(int plan);
    int reserve(const std::shared_ptr<flash::DisplayObject>& mc, int plan);
};

}  // namespace mt
