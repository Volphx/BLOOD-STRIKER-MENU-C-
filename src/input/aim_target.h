#pragma once
#include "../core/types.h"
#include <cctype>

inline int aimBonePriority(std::string name, int region) {
    for (auto &c : name)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (region == 0)
        return name.find("head") != std::string::npos && name.find("end") == std::string::npos ? 1 : 0;
    if (region == 1)
        return name.find("neck") != std::string::npos ? 1 : 0;
    if (region == 2) {
        if (name.find("chest") != std::string::npos || name.find("spine2") != std::string::npos ||
            name.find("spine 2") != std::string::npos || name.find("spine_02") != std::string::npos)
            return 3;
        if (name.find("spine1") != std::string::npos || name.find("spine 1") != std::string::npos)
            return 2;
        return name.find("spine") != std::string::npos ? 1 : 0;
    }
    if (region == 3)
        return name.find("pelvis") != std::string::npos ? 1 : 0;
    return 0;
}

inline bool selectedAimPoint(const Entity &entity, int region, Vec3 &point) {
    if (region < 0 || region >= static_cast<int>(entity.aimPoints.size()) || !entity.aimPointsKnown[region])
        return false;
    point = entity.aimPoints[region];
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}
