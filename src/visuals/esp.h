#pragma once
#include "../game/game_reader.h"
#include "projection.h"
#include <algorithm>
#include <array>
#include <cstdio>
inline void espText(ImDrawList *draw, ImVec2 pos, ImU32 color, const char *text, bool centered = false) {
    constexpr float fontSize = 14.0f;
    const auto extent = ImGui::GetFont()->CalcTextSizeA(fontSize, 10000, 0, text);
    if (centered)
        pos.x -= extent.x * 0.5f;
    pos.x = std::round(pos.x);
    pos.y = std::round(pos.y);
    draw->AddText(nullptr, fontSize, pos, color, text);
}

struct EspSegment {
    ImVec2 a, b;
};
inline std::array<EspSegment, 8> espCorners(ImVec2 lo, ImVec2 hi) {
    const float length = (std::min)(16.0f, (std::min)(hi.x - lo.x, hi.y - lo.y) * 0.28f);
    return {{{lo, {lo.x + length, lo.y}},
             {lo, {lo.x, lo.y + length}},
             {{hi.x, lo.y}, {hi.x - length, lo.y}},
             {{hi.x, lo.y}, {hi.x, lo.y + length}},
             {{lo.x, hi.y}, {lo.x + length, hi.y}},
             {{lo.x, hi.y}, {lo.x, hi.y - length}},
             {hi, {hi.x - length, hi.y}},
             {hi, {hi.x, hi.y - length}}}};
}

inline void drawFeatures(GameReader &game, const Options &cfg, bool valid, bool menuOpen) {
    ImVec2 size = ImGui::GetIO().DisplaySize, center(size.x / 2, size.y / 2);
    auto *draw = ImGui::GetBackgroundDrawList();
    if (cfg.aim)
        draw->AddCircle(center, cfg.radius, IM_COL32(210, 225, 240, 140), 96, 1);
    if (cfg.performanceHud) {
        char hud[140];
        sprintf_s(hud, "ESP %.0f FPS  |  leitura %.2f ms  |  %zu entidades%s", ImGui::GetIO().Framerate,
                  game.lastReadMs, game.entities.size(), game.discoveryBusy() ? "  |  buscando..." : "");
        const auto extent = ImGui::CalcTextSize(hud);
        ImVec2 pos((std::max)(8.0f, size.x - extent.x - 26), 12);
        draw->AddRectFilled(ImVec2(pos.x - 8, pos.y - 5), ImVec2(size.x - 8, pos.y + 20),
                            IM_COL32(17, 24, 35, 230), 5);
        espText(draw, pos, IM_COL32(200, 220, 235, 255), hud);
    }
    if (!valid)
        return;
    float best = cfg.radius * cfg.radius;
    ImVec2 target{};
    bool found = false;
    for (const auto &e : game.entities) {
        const auto entityColor = e.downed ? ImVec4(1.0f, 0.18f, 0.18f, 1) : cfg.color;
        const auto color = ImGui::ColorConvertFloat4ToU32(entityColor);
        const auto textColor = e.downed ? color : IM_COL32_WHITE;
        if (skipDeadEntity(e, cfg))
            continue;
        if (skipTeamEntity(e, cfg, game.localTeam, game.localTeamValid, game.trainingMode))
            continue;
        if (e.distanceKnown && e.distance > cfg.maxDistance)
            continue;
        ImVec2 head, feet;
        if (!project(e.head, game.matrix, size, head) || !project(e.feet, game.matrix, size, feet))
            continue;
        const float height = std::abs(feet.y - head.y), width = height * 0.44f;
        const float middle = (head.x + feet.x) * 0.5f;
        ImVec2 lo(std::round(middle - width / 2), std::round((std::min)(head.y, feet.y)));
        ImVec2 hi(std::round(middle + width / 2), std::round((std::max)(head.y, feet.y)));
        if (hi.x < 0 || lo.x > size.x || hi.y < 0 || lo.y > size.y)
            continue;
        if (cfg.esp && height > 3 && height < size.y * 2) {
            if (cfg.lines) {
                const ImVec2 origin(center.x, cfg.lineOrigin == 0   ? size.y - 12
                                              : cfg.lineOrigin == 1 ? center.y
                                                                    : 12);
                const ImVec2 destination(middle, hi.y);
                draw->AddLine(origin, destination, color, cfg.thickness);
            }
            if (cfg.boxes) {
                if (cfg.filled)
                    draw->AddRectFilled(
                        lo, hi,
                        ImGui::GetColorU32(ImVec4(entityColor.x, entityColor.y, entityColor.z, 0.09f)));
                if (cfg.boxStyle == 0) {
                    draw->AddRect(lo, hi, color, 0, 0, cfg.thickness);
                } else {
                    for (const auto &segment : espCorners(lo, hi)) {
                        draw->AddLine(segment.a, segment.b, color, cfg.thickness);
                    }
                }
            }
            if (cfg.headDot) {
                draw->AddCircle(head, 3.5f, color, 16, cfg.thickness);
            }
            if (cfg.skeleton && !e.bones.empty()) {
                const auto boneColor = e.downed ? color : ImGui::ColorConvertFloat4ToU32(cfg.skeletonColor);
                for (size_t i = 0; i < e.bones.size(); ++i) {
                    const auto &bone = e.bones[i];
                    if (bone.parent <= 0 || bone.parent >= static_cast<int>(e.bones.size()))
                        continue;
                    const auto &parent = e.bones[static_cast<size_t>(bone.parent)];
                    const float dx = bone.position.x - parent.position.x,
                                dy = bone.position.y - parent.position.y,
                                dz = bone.position.z - parent.position.z;
                    if (dx * dx + dy * dy + dz * dz < 0.0001f || dx * dx + dy * dy + dz * dz > 4.0f)
                        continue;
                    ImVec2 a, b;
                    if (!project(bone.position, game.matrix, size, a) ||
                        !project(parent.position, game.matrix, size, b))
                        continue;
                    if (std::abs(a.x - b.x) > size.x * 0.5f || std::abs(a.y - b.y) > size.y * 0.5f)
                        continue;
                    const float sx = a.x - b.x, sy = a.y - b.y;
                    if (sx * sx + sy * sy < 1.0f)
                        continue;
                    draw->AddLine(a, b, boneColor, cfg.skeletonThickness);
                }
            }
            const bool validHealth = cfg.healthBar && e.healthKnown && std::isfinite(e.health) &&
                                     std::isfinite(e.maxHealth) && e.maxHealth > 0;
            const int barPosition = std::clamp(cfg.healthPosition, 0, 3);
            const float nameY = lo.y - (validHealth && barPosition == 2 ? 25.0f : 17.0f);
            if (cfg.names && !e.name.empty())
                espText(draw, {middle, nameY}, textColor, e.name.c_str(), true);
            else if (cfg.showIds) {
                char id[48];
                sprintf_s(id, "ID %llX", static_cast<unsigned long long>(e.id));
                espText(draw, {middle, nameY}, textColor, id, true);
            }
            if (validHealth) {
                const float ratio = std::clamp(e.health / e.maxHealth, 0.0f, 1.0f);
                // Continuous red -> yellow -> green, using the current HP maximum.
                const float blend = ratio < 0.5f ? ratio * 2.0f : (ratio - 0.5f) * 2.0f;
                const ImVec4 low =
                    ratio < 0.5f ? ImVec4(1.0f, 0.22f, 0.22f, 1) : ImVec4(1.0f, 0.82f, 0.20f, 1);
                const ImVec4 high =
                    ratio < 0.5f ? ImVec4(1.0f, 0.82f, 0.20f, 1) : ImVec4(0.20f, 0.88f, 0.40f, 1);
                const auto hpColor = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(low.x + (high.x - low.x) * blend, low.y + (high.y - low.y) * blend,
                           low.z + (high.z - low.z) * blend, 1));
                ImVec2 barLo, barHi;
                if (barPosition == 0) {
                    barLo = {lo.x - 6, lo.y};
                    barHi = {lo.x - 3, hi.y};
                } else if (barPosition == 1) {
                    barLo = {hi.x + 3, lo.y};
                    barHi = {hi.x + 6, hi.y};
                } else if (barPosition == 2) {
                    barLo = {lo.x, lo.y - 6};
                    barHi = {hi.x, lo.y - 3};
                } else {
                    barLo = {lo.x, hi.y + 4};
                    barHi = {hi.x, hi.y + 7};
                }
                draw->AddRectFilled(barLo, barHi, IM_COL32(255, 255, 255, 42), 1);
                if (ratio > 0) {
                    ImVec2 fillLo = barLo, fillHi = barHi;
                    if (barPosition <= 1)
                        fillLo.y = barHi.y - (barHi.y - barLo.y) * ratio;
                    else
                        fillHi.x = barLo.x + (barHi.x - barLo.x) * ratio;
                    draw->AddRectFilled(fillLo, fillHi, hpColor, 1);
                }
            }
            std::string details;
            char part[64];
            if (cfg.distances && e.distanceKnown) {
                sprintf_s(part, "%.0f m", e.distance);
                details = part;
            }
            if (cfg.healthBar) {
                if (!details.empty())
                    details += " | ";
                if (e.healthKnown) {
                    if (e.maxHealth > 0)
                        sprintf_s(part, "HP %.0f/%.0f", e.health, e.maxHealth);
                    else
                        sprintf_s(part, "HP %.0f", e.health);
                    details += part;
                } else
                    details += "HP indisponivel";
            }
            if (!details.empty())
                espText(draw, {middle, hi.y + (validHealth && barPosition == 3 ? 12.0f : 4.0f)}, textColor,
                        details.c_str(), true);
        }
        Vec3 aimWorld{};
        ImVec2 aimScreen{};
        if (!selectedAimPoint(e, cfg.aimTarget, aimWorld) || !project(aimWorld, game.matrix, size, aimScreen))
            continue;
        const float dx = aimScreen.x - center.x, dy = aimScreen.y - center.y, distance = dx * dx + dy * dy;
        if (distance < best && aimScreen.x >= 0 && aimScreen.x <= size.x && aimScreen.y >= 0 &&
            aimScreen.y <= size.y) {
            best = distance;
            target = aimScreen;
            found = true;
        }
    }
    DWORD foregroundPid{};
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
    if (cfg.aim && found && !menuOpen && foregroundPid == game.processId() &&
        (GetAsyncKeyState(VK_RBUTTON) & 0x8000)) {
        draw->AddCircle(target, 5, IM_COL32(255, 100, 100, 255));
        const float gain = 1 - std::exp(-ImGui::GetIO().DeltaTime * 60 / (std::max)(cfg.smoothing, 1.0f));
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        input.mi.dx = static_cast<LONG>(std::round((target.x - center.x) * gain));
        input.mi.dy = static_cast<LONG>(std::round((target.y - center.y) * gain));
        if (input.mi.dx || input.mi.dy)
            SendInput(1, &input, sizeof(input));
    }
}
