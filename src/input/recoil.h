#pragma once
#include <algorithm>
#include <cmath>
#include <utility>

// Relative mouse counts, accumulated independently of render frequency.
struct RecoilAccumulator {
    double x = 0, y = 0;
    std::pair<long, long> step(float seconds, bool active, float horizontal, float vertical) {
        if (!active || !std::isfinite(seconds) || seconds <= 0 || !std::isfinite(horizontal) ||
            !std::isfinite(vertical)) {
            x = y = 0;
            return {0, 0};
        }
        const double dt = std::min(static_cast<double>(seconds), 0.05);
        x += std::clamp(horizontal, -400.0f, 400.0f) * dt;
        y += std::clamp(vertical, 0.0f, 800.0f) * dt;
        const auto dx = static_cast<long>(std::trunc(x)), dy = static_cast<long>(std::trunc(y));
        x -= dx;
        y -= dy;
        return {dx, dy};
    }
};
