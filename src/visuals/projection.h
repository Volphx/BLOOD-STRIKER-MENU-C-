#pragma once
#include "../core/types.h"
inline bool project(const Vec3 &p, const float *m, ImVec2 size, ImVec2 &screen) {
    const float x = m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3];
    const float y = m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7];
    const float w = m[12] * p.x + m[13] * p.y + m[14] * p.z + m[15];
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || w <= 0.001f)
        return false;
    screen = ImVec2((x / w + 1) * size.x * 0.5f, (1 - y / w) * size.y * 0.5f);
    return std::isfinite(screen.x) && std::isfinite(screen.y);
}
