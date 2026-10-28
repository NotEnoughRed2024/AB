// mt.Timer: the original never calls update(), so tmod is 1 except where the game itself changes it.
#pragma once

namespace mt {
struct Timer {
    static inline double tmod = 1.0;
};
}  // namespace mt
