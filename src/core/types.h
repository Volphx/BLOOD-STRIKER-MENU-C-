#pragma once
#include "imgui.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
struct Vec3 {
    float x{}, y{}, z{};
};
struct BonePose {
    Vec3 position;
    int parent = -1;
};
enum class LifeState { Unknown, Alive, Dead };
struct Entity {
    Vec3 feet, head;
    float health{};
    int team{};
    bool healthKnown = false, teamKnown = false, downed = false;
    float maxHealth = 0, distance = 0;
    bool distanceKnown = false;
    uintptr_t id = 0;
    std::string name;
    std::vector<BonePose> bones;
    std::array<Vec3, 4> aimPoints{};
    std::array<bool, 4> aimPointsKnown{};
    LifeState life = LifeState::Unknown;
};
struct Options {
    bool esp = true, aim = false, teamFilter = true;
    bool recoil = false;
    float recoilVertical = 90.0f, recoilHorizontal = 0.0f;
    bool boxes = true;
    bool lines = true, names = true, healthBar = true, distances = true;
    bool filled = false, headDot = false, showIds = false;
    bool skeleton = true;
    bool ignoreDead = true;
    bool vsync = false, performanceHud = true;
    int boxStyle = 1, lineOrigin = 0, frameLimit = 240;
    int healthPosition = 0; // left, right, top, bottom
    float thickness = 0.8f, maxDistance = 250;
    float skeletonThickness = 0.8f;
    ImVec4 skeletonColor = ImVec4(1, 1, 1, 1);
    float radius = 120, smoothing = 8;
    int aimTarget = 0;
    ImVec4 color = ImVec4(1, 1, 1, 1);
};
inline bool skipDeadEntity(const Entity &entity, const Options &options) {
    return options.ignoreDead && (entity.life == LifeState::Dead ||
                                  (entity.healthKnown && std::isfinite(entity.health) && entity.health <= 0));
}
inline bool skipTeamEntity(const Entity &entity, const Options &options, int localTeam, bool localKnown,
                           bool trainingMode = false) {
    return !trainingMode && options.teamFilter &&
           (!localKnown || !entity.teamKnown || entity.team == localTeam);
}
