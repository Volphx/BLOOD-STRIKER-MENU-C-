#pragma once
#include "../core/types.h"
#include "../input/aim_target.h"
#include "imgui.h"
#include "offsets.h"
#include "python_profiles.h"
#include <TlHelp32.h>
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class GameReader {
    HANDLE process = nullptr;
    DWORD pid = 0;
    uintptr_t base = 0;
    uintptr_t moduleSize = 0;
    std::wstring ini;
    uintptr_t offset(const wchar_t *key, const wchar_t *section = L"layout") const {
        wchar_t value[80]{};
        GetPrivateProfileStringW(section, key, L"0", value, 80, ini.c_str());
        return static_cast<uintptr_t>(_wcstoui64(value, nullptr, 0));
    }
    template <class T> bool read(uintptr_t address, T &out) const {
        SIZE_T copied = 0;
        return address >= 0x10000 &&
               ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address), &out, sizeof(T), &copied) &&
               copied == sizeof(T);
    }
    uintptr_t matrixRva{}, listRva{}, countRva{}, localRva{};
    uintptr_t feetOff{}, headOff{}, healthOff{}, maxHealthOff{}, teamOff{};
    bool enabled = false;
    bool snapshotMode = false;
    bool snapshotScanned = false;
    DWORD snapshotPid = 0;
    uintptr_t snapshotBase = 0, cameraAddress = 0, cameraVtable = 0, entityVtable = 0;
    std::vector<uintptr_t> snapshotEntities;
    std::vector<uintptr_t> sceneWorlds, cameraCandidates;
    std::vector<uintptr_t> bootstrapEntities;
    struct SceneDiscovery {
        std::vector<uintptr_t> entities, worlds, cameras;
    };
    std::future<SceneDiscovery> discoveryFuture;
    std::unordered_map<uintptr_t, std::pair<bool, ULONGLONG>> rigClasses;
    std::unordered_map<uintptr_t, std::array<int, 4>> aimRigIndices;
    ULONGLONG sceneRefreshTick = 0, recoveryTick = 0;
    PythonProfiles profiles;
    uint64_t epoch = 0, discoveryEpoch = 0;
    void resetSceneState() {
        ++epoch;
        snapshotScanned = false;
        cameraAddress = 0;
        snapshotEntities.clear();
        sceneWorlds.clear();
        cameraCandidates.clear();
        bootstrapEntities.clear();
        rigClasses.clear();
        aimRigIndices.clear();
        sceneRefreshTick = recoveryTick = 0;
        sceneEntityCount = 0;
        entities.clear();
        localTeamValid = trainingMode = false;
        knownTeamEntities = 0;
        profiles.reset();
    }
    bool bindCurrentSession() {
        char cookie[8]{};
        uint64_t version{};
        uintptr_t cameraFunction{}, entityFunction{};
        if (moduleSize < sizeof(uintptr_t) || offsets::python::RuntimeRva + 24 > moduleSize ||
            cameraVtable > moduleSize - sizeof(uintptr_t) || entityVtable > moduleSize - sizeof(uintptr_t) ||
            !read(base + offsets::python::RuntimeRva, cookie) || std::memcmp(cookie, "xdebugpy", 8) ||
            !read(base + offsets::python::RuntimeRva + offsets::python::RuntimeVersion, version) ||
            version != offsets::python::ExpectedVersion || !read(base + cameraVtable, cameraFunction) ||
            !read(base + entityVtable, entityFunction) || cameraFunction < base ||
            cameraFunction >= base + moduleSize || entityFunction < base ||
            entityFunction >= base + moduleSize) {
            status = "Compilacao do jogo incompativel com os offsets configurados";
            return false;
        }
        if (snapshotPid != pid || snapshotBase != base) {
            resetSceneState();
            snapshotPid = pid;
            snapshotBase = base;
            status = "Nova sessao detectada; redescobrindo camera e alvos";
        }
        return true;
    }
    std::string boneName(uintptr_t resource) const {
        uintptr_t symbol{};
        std::array<unsigned char, 24> header{};
        if (!read(resource + offsets::engine::BoneNameSymbol, symbol) || !read(symbol, header))
            return {};
        uint32_t length{};
        std::memcpy(&length, header.data() + offsets::engine::SymbolLength, 4);
        if (!length || length > 127)
            return {};
        std::string name(length, '\0');
        SIZE_T copied = 0;
        if (!ReadProcessMemory(process, reinterpret_cast<LPCVOID>(symbol + offsets::engine::SymbolText),
                               name.data(), length, &copied) ||
            copied != length)
            return {};
        for (auto &c : name)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return name;
    }
    bool humanoidRig(uintptr_t component) {
        uintptr_t actor{}, cache{};
        std::array<uintptr_t, 2> range{};
        if (!read(component + offsets::engine::ActorLink, actor) ||
            !read(actor + offsets::engine::PoseCache, cache) ||
            !read(cache + offsets::engine::PoseVector, range) || range[0] < 0x10000 || range[1] < range[0])
            return false;
        const auto bytes = range[1] - range[0];
        if (bytes % offsets::engine::BoneStride || bytes / offsets::engine::BoneStride < 15 ||
            bytes / offsets::engine::BoneStride > 512)
            return false;
        uintptr_t first{};
        if (!read(range[0] + offsets::engine::BoneResource, first) || !first)
            return false;
        const auto now = GetTickCount64();
        auto cached = rigClasses.find(first);
        if (cached != rigClasses.end() && now - cached->second.second < 10000)
            return cached->second.first;
        bool head = false, torso = false, legs = false;
        for (uintptr_t i = 0; i < bytes / offsets::engine::BoneStride; ++i) {
            uintptr_t resource{};
            if (!read(range[0] + i * offsets::engine::BoneStride + offsets::engine::BoneResource, resource))
                continue;
            const auto name = boneName(resource);
            head |= name.find("head") != std::string::npos;
            torso |= name.find("pelvis") != std::string::npos || name.find("hip") != std::string::npos ||
                     name.find("spine") != std::string::npos;
            legs |= name.find("thigh") != std::string::npos || name.find("calf") != std::string::npos ||
                    name.find("foot") != std::string::npos || name.find("leg") != std::string::npos;
            if (head && torso && legs)
                break;
        }
        const bool human = head && torso && legs;
        rigClasses[first] = {human, now};
        return human;
    }
    void listValues(uintptr_t head, size_t limit, std::vector<uintptr_t> &out) const {
        uintptr_t at{};
        if (head < 0x10000 || !read(head, at))
            return;
        std::unordered_set<uintptr_t> visited;
        while (at >= 0x10000 && at != head && visited.size() < limit && visited.insert(at).second) {
            std::array<uintptr_t, 3> node{};
            if (!read(at, node))
                break;
            if (node[2] >= 0x10000)
                out.push_back(node[2]);
            at = node[0];
        }
    }
    void refreshSceneEntities() {
        const auto now = GetTickCount64();
        if (now - sceneRefreshTick < 250)
            return;
        sceneRefreshTick = now;
        if (sceneWorlds.empty()) {
            for (auto entity : snapshotEntities) {
                uintptr_t world{};
                if (read(entity + offsets::engine::EntityWorld, world) && world)
                    sceneWorlds.push_back(world);
            }
            std::sort(sceneWorlds.begin(), sceneWorlds.end());
            sceneWorlds.erase(std::unique(sceneWorlds.begin(), sceneWorlds.end()), sceneWorlds.end());
        }
        std::vector<uintptr_t> worlds, found;
        for (auto world : sceneWorlds) {
            uintptr_t vt{}, head{};
            if (!read(world, vt) || vt != base + offsets::engine::WorldVtableRva ||
                !read(world + offsets::engine::WorldLevelsHead, head))
                continue;
            std::vector<uintptr_t> levels;
            listValues(head, 512, levels);
            if (levels.empty())
                continue;
            worlds.push_back(world);
            std::vector<uintptr_t> areas;
            for (auto level : levels) {
                uintptr_t root{};
                if (read(level + offsets::engine::LevelRootArea, root) && root)
                    areas.push_back(root);
            }
            std::unordered_set<uintptr_t> visited;
            for (size_t i = 0; i < areas.size() && visited.size() < 4096; ++i) {
                const auto area = areas[i];
                if (!visited.insert(area).second)
                    continue;
                uintptr_t entitiesHead{}, connections{};
                if (read(area + offsets::engine::AreaEntitiesHead, entitiesHead))
                    listValues(entitiesHead, 20000, found);
                if (read(area + offsets::engine::AreaConnectionsHead, connections))
                    listValues(connections, 4096, areas);
                if (found.size() > 100000)
                    break;
            }
        }
        if (!worlds.empty()) {
            found.insert(found.end(), bootstrapEntities.begin(), bootstrapEntities.end());
            std::sort(found.begin(), found.end());
            found.erase(std::unique(found.begin(), found.end()), found.end());
            sceneEntityCount = found.size();
            found.erase(std::remove_if(
                            found.begin(), found.end(),
                            [&](uintptr_t entity) {
                                std::array<unsigned char, offsets::engine::EntityHeaderSize> data{};
                                uintptr_t vt{}, world{}, components{}, skeleton{};
                                if (!read(entity, data))
                                    return true;
                                std::memcpy(&vt, data.data(), 8);
                                std::memcpy(&world, data.data() + offsets::engine::EntityWorld, 8);
                                std::memcpy(&components, data.data() + offsets::engine::EntityComponents, 8);
                                const auto slot = data[offsets::engine::SkeletonSlot];
                                return vt != base + entityVtable || !world || !(slot & 128) || !components ||
                                       !read(components + (slot & 127) * 8, skeleton) || !skeleton ||
                                       !humanoidRig(skeleton);
                            }),
                        found.end());
            snapshotEntities = std::move(found);
            sceneWorlds = std::move(worlds);
        } else {
            sceneWorlds.clear();
            if (now - recoveryTick > 3000)
                requestDiscovery();
        }
    }
    bool recoverCamera() {
        uintptr_t best = 0;
        float bestFar = 0;
        for (auto candidate : cameraCandidates) {
            uintptr_t vt{};
            float farPlane{}, fov{};
            uint16_t dimensions[2]{};
            if (!read(candidate, vt) || vt != base + cameraVtable ||
                !read(candidate + offsets::engine::CameraFarPlane, farPlane) ||
                !read(candidate + offsets::engine::CameraFov, fov) ||
                !read(candidate + offsets::engine::CameraDimensions, dimensions))
                continue;
            if (std::isfinite(farPlane) && farPlane > bestFar && std::isfinite(fov) && fov > 10 &&
                fov < 150 && dimensions[0] >= 100 && dimensions[1] >= 100) {
                best = candidate;
                bestFar = farPlane;
            }
        }
        if (best) {
            cameraAddress = best;
            return true;
        }
        if (GetTickCount64() - recoveryTick > 3000)
            requestDiscovery();
        return false;
    }
    bool readBonePose(uintptr_t entityAddress, uintptr_t component,
                      const std::array<unsigned char, offsets::engine::EntityHeaderSize> &entityBytes,
                      Entity &entity) {
        // ActorComponent::GetBoneWorldTransform follows +E8, then +D8, to
        // the pose cache. Cache nodes have stride A0, transform at +30,
        // and cache-parent index at +94 (not a guessed anatomical graph).
        std::array<unsigned char, offsets::engine::ActorHeaderSize> actorBytes{};
        uintptr_t owner{}, actor{}, cache{};
        if (!read(component, actorBytes))
            return false;
        std::memcpy(&owner, actorBytes.data() + offsets::engine::ActorOwner, 8);
        std::memcpy(&actor, actorBytes.data() + offsets::engine::ActorLink, 8);
        if (owner != entityAddress || actor < 0x10000 || !read(actor + offsets::engine::PoseCache, cache) ||
            cache < 0x10000)
            return false;
        std::array<uintptr_t, 3> vector{};
        if (!read(cache + offsets::engine::PoseVector, vector) || vector[0] < 0x10000 ||
            vector[1] < vector[0] || vector[2] < vector[1])
            return false;
        const uintptr_t bytes = vector[1] - vector[0];
        if (bytes % offsets::engine::BoneStride || bytes / offsets::engine::BoneStride < 2 ||
            bytes / offsets::engine::BoneStride > 512)
            return false;
        const auto count = static_cast<size_t>(bytes / offsets::engine::BoneStride);
        std::vector<unsigned char> nodes(static_cast<size_t>(bytes));
        SIZE_T copied = 0;
        if (!ReadProcessMemory(process, reinterpret_cast<LPCVOID>(vector[0]), nodes.data(), nodes.size(),
                               &copied) ||
            copied != nodes.size())
            return false;
        float world[12]{};
        std::memcpy(world, entityBytes.data() + offsets::engine::EntityTransform, sizeof(world));
        for (float v : world)
            if (!std::isfinite(v))
                return false;
        std::vector<BonePose> result(count);
        for (size_t i = 0; i < count; ++i) {
            Vec3 local{};
            int16_t parent{};
            std::memcpy(&local,
                        nodes.data() + i * offsets::engine::BoneStride + offsets::engine::BonePosition,
                        sizeof(local));
            std::memcpy(&parent, nodes.data() + i * offsets::engine::BoneStride + offsets::engine::BoneParent,
                        sizeof(parent));
            if (parent < -1 || parent >= static_cast<int>(count) || parent == static_cast<int>(i) ||
                !std::isfinite(local.x) || !std::isfinite(local.y) || !std::isfinite(local.z) ||
                std::abs(local.x) > 20 || std::abs(local.y) > 20 || std::abs(local.z) > 20)
                return false;
            result[i].parent = parent;
            result[i].position = {local.x * world[0] + local.y * world[3] + local.z * world[6] + world[9],
                                  local.x * world[1] + local.y * world[4] + local.z * world[7] + world[10],
                                  local.x * world[2] + local.y * world[5] + local.z * world[8] + world[11]};
            const auto &point = result[i].position;
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
                return false;
        }
        // Reject corrupt or cyclic parent graphs instead of drawing artifacts.
        for (size_t i = 0; i < count; ++i) {
            int at = static_cast<int>(i);
            size_t depth = 0;
            while (at >= 0 && depth <= count) {
                at = result[static_cast<size_t>(at)].parent;
                ++depth;
            }
            if (depth > count)
                return false;
        }
        uintptr_t rigResource{};
        std::memcpy(&rigResource, nodes.data() + offsets::engine::BoneResource, sizeof(rigResource));
        auto cachedAim = aimRigIndices.find(rigResource);
        if (rigResource && cachedAim == aimRigIndices.end()) {
            std::array<int, 4> indices{{-1, -1, -1, -1}}, scores{};
            for (size_t i = 0; i < count; ++i) {
                uintptr_t resource{};
                std::memcpy(&resource,
                            nodes.data() + i * offsets::engine::BoneStride + offsets::engine::BoneResource,
                            sizeof(resource));
                const auto name = boneName(resource);
                for (int region = 0; region < 4; ++region) {
                    const auto score = aimBonePriority(name, region);
                    if (score > scores[region]) {
                        scores[region] = score;
                        indices[region] = static_cast<int>(i);
                    }
                }
            }
            cachedAim = aimRigIndices.emplace(rigResource, indices).first;
        }
        if (cachedAim != aimRigIndices.end())
            for (size_t region = 0; region < 4; ++region) {
                const auto index = cachedAim->second[region];
                if (index >= 0 && static_cast<size_t>(index) < result.size()) {
                    entity.aimPoints[region] = result[index].position;
                    entity.aimPointsKnown[region] = true;
                }
            }
        entity.bones = std::move(result);
        return true;
    }
    void pollDiscovery() {
        if (!discoveryFuture.valid() ||
            discoveryFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return;
        try {
            auto found = discoveryFuture.get();
            if (discoveryEpoch == epoch) {
                snapshotEntities = std::move(found.entities);
                sceneWorlds = std::move(found.worlds);
                cameraCandidates = std::move(found.cameras);
                bootstrapEntities = snapshotEntities;
                snapshotScanned = true;
                sceneRefreshTick = 0;
                rigClasses.clear();
                aimRigIndices.clear();
            }
        } catch (...) {
            status = "Falha na busca de entidades; tente novamente";
        }
    }
    bool updateSnapshot() {
        if (process) {
            DWORD exitCode{};
            if (!GetExitCodeProcess(process, &exitCode) || exitCode != STILL_ACTIVE)
                disconnect();
        }
        if (!process && !attach())
            return false;
        if (pid != snapshotPid || base != snapshotBase) {
            status = "Sessao diferente do dump. Enderecos de heap precisam ser redescobertos.";
            return false;
        }
        uintptr_t vptr{};
        float view[12]{}, fov{};
        Vec3 cameraPosition{};
        uint16_t dimensions[2]{};
        std::array<unsigned char, offsets::engine::CameraHeaderSize> cameraBytes{};
        if (!read(cameraAddress, cameraBytes) ||
            (std::memcpy(&vptr, cameraBytes.data(), sizeof(vptr)), vptr != base + cameraVtable)) {
            if (recoverCamera() && read(cameraAddress, cameraBytes)) {
            } else {
                status = "Camera da sessao nao esta mais valida";
                return false;
            }
        }
        auto cameraField = [&](size_t at, auto &value) {
            std::memcpy(&value, cameraBytes.data() + at, sizeof(value));
        };
        cameraField(0, vptr);
        cameraField(offsets::engine::CameraView, view);
        cameraField(offsets::engine::CameraFov, fov);
        cameraField(offsets::engine::WorldPosition, cameraPosition);
        cameraField(offsets::engine::CameraDimensions, dimensions);
        if (vptr != base + cameraVtable) {
            status = "Tipo da camera invalido";
            return false;
        }
        if (!std::isfinite(cameraPosition.x) || !std::isfinite(cameraPosition.y) ||
            !std::isfinite(cameraPosition.z)) {
            status = "Posicao da camera invalida";
            return false;
        }
        for (float v : view)
            if (!std::isfinite(v)) {
                status = "Camera invalida";
                return false;
            }
        if (!std::isfinite(fov) || fov < 10 || fov > 150 || dimensions[0] < 100 || dimensions[1] < 100) {
            status = "FOV ou dimensoes da camera invalidos";
            return false;
        }
        // Engine view is a row-vector 4x3 affine matrix. Camera looks along -Z.
        const float sy = 1.0f / std::tan(fov * 0.00872664626f);
        const float sx = sy * static_cast<float>(dimensions[1]) / dimensions[0];
        for (auto &v : matrix)
            v = 0;
        matrix[0] = view[0] * sx;
        matrix[1] = view[3] * sx;
        matrix[2] = view[6] * sx;
        matrix[3] = view[9] * sx;
        matrix[4] = view[1] * sy;
        matrix[5] = view[4] * sy;
        matrix[6] = view[7] * sy;
        matrix[7] = view[10] * sy;
        matrix[12] = -view[2];
        matrix[13] = -view[5];
        matrix[14] = -view[8];
        matrix[15] = -view[11];
        if (!snapshotScanned)
            snapshotScanned = requestDiscovery();
        refreshSceneEntities();
        profiles.refresh(
            base, base + entityVtable, GetTickCount64(), [this](uintptr_t at, void *dst, size_t bytes) {
                SIZE_T copied = 0;
                return at >= 0x10000 &&
                       ReadProcessMemory(process, reinterpret_cast<LPCVOID>(at), dst, bytes, &copied) &&
                       copied == bytes;
            });
        localTeam = profiles.localFaction;
        localTeamValid = profiles.localValid;
        trainingMode = profiles.trainingMode;
        knownTeamEntities = 0;
        for (auto address : snapshotEntities) {
            if (profiles.localNative && address == profiles.localNative)
                continue;
            Entity e{};
            uintptr_t world{}, components{}, skeleton{};
            uint8_t slot{};
            std::array<unsigned char, offsets::engine::EntityHeaderSize> entityBytes{};
            if (!read(address, entityBytes))
                continue;
            std::memcpy(&vptr, entityBytes.data(), sizeof(vptr));
            std::memcpy(&world, entityBytes.data() + offsets::engine::EntityWorld, sizeof(world));
            std::memcpy(&components, entityBytes.data() + offsets::engine::EntityComponents,
                        sizeof(components));
            std::memcpy(&e.feet, entityBytes.data() + offsets::engine::WorldPosition, sizeof(e.feet));
            slot = entityBytes[offsets::engine::SkeletonSlot];
            if (vptr != base + entityVtable || !world || !(slot & 0x80) || !components ||
                !read(components + (slot & 0x7f) * sizeof(uintptr_t), skeleton) || !skeleton)
                continue;
            if (!std::isfinite(e.feet.x) || !std::isfinite(e.feet.y) || !std::isfinite(e.feet.z) ||
                std::abs(e.feet.x) > 100000 || std::abs(e.feet.y) > 100000 || std::abs(e.feet.z) > 100000)
                continue;
            if (!humanoidRig(skeleton))
                continue;
            const float dx = e.feet.x - cameraPosition.x, dy = e.feet.y - cameraPosition.y,
                        dz = e.feet.z - cameraPosition.z;
            // Exclude a body directly under the eye, not every close-range target.
            if (dx * dx + dz * dz < 0.1225f && dy > 0.1f && dy < 3.0f)
                continue;
            e.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            e.distanceKnown = true;
            e.id = address;
            e.head = e.feet;
            e.head.y += 1.65f; // Estimated standing height, not a resolved bone.
            e.healthKnown = false;
            const auto team = profiles.entities.find(address);
            if (team != profiles.entities.end() && localTeamValid) {
                e.team = team->second.faction;
                e.teamKnown = true;
                ++knownTeamEntities;
                e.name = team->second.name;
                e.healthKnown = team->second.healthKnown;
                e.health = team->second.health;
                e.maxHealth = team->second.maxHealth;
                e.downed = team->second.combatState == 1;
                if (team->second.combatState >= 0)
                    e.life = team->second.combatState <= 1 ? LifeState::Alive : LifeState::Dead;
            }
            if (readSkeletons)
                readBonePose(address, skeleton, entityBytes, e);
            entities.push_back(e);
        }
        if ((entities.empty() && GetTickCount64() - recoveryTick > 3000) ||
            GetTickCount64() - recoveryTick > 15000)
            requestDiscovery();
        status = profiles.status + ". Perfis associados: " + std::to_string(knownTeamEntities) + "/" +
                 std::to_string(entities.size());
        return true;
    }

  public:
    std::string sceneDiagnostics() {
        std::string out = "Bootstrap=" + std::to_string(bootstrapEntities.size()) +
                          " Worlds=" + std::to_string(sceneWorlds.size()) +
                          " Cameras=" + std::to_string(cameraCandidates.size()) + "\n";
        for (auto entity : bootstrapEntities) {
            uintptr_t world{}, components{}, sk{}, actor{}, cache{};
            uint8_t slot{};
            Vec3 pos{};
            char line[256];
            if (!read(entity + offsets::engine::EntityWorld, world) ||
                !read(entity + offsets::engine::EntityComponents, components) ||
                !read(entity + offsets::engine::SkeletonSlot, slot) ||
                !read(entity + offsets::engine::WorldPosition, pos))
                continue;
            if (!(slot & 128) || !read(components + (slot & 127) * 8, sk))
                continue;
            read(sk + offsets::engine::ActorLink, actor);
            read(actor + offsets::engine::PoseCache, cache);
            std::array<uintptr_t, 2> range{};
            read(cache + offsets::engine::PoseVector, range);
            sprintf_s(line, "Entity %llX World %llX XYZ %.2f %.2f %.2f Rig=%d Nodes=%llu\n",
                      static_cast<unsigned long long>(entity), static_cast<unsigned long long>(world), pos.x,
                      pos.y, pos.z, int(humanoidRig(sk)),
                      static_cast<unsigned long long>(
                          range[1] >= range[0] ? (range[1] - range[0]) / offsets::engine::BoneStride : 0));
            out += line;
            if (range[1] >= range[0] && (range[1] - range[0]) / offsets::engine::BoneStride <= 512)
                for (size_t i = 0; i < (range[1] - range[0]) / offsets::engine::BoneStride; ++i) {
                    uintptr_t resource{};
                    if (read(range[0] + i * offsets::engine::BoneStride + offsets::engine::BoneResource,
                             resource))
                        out += boneName(resource) + ", ";
                }
            out += "\n";
        }
        return out;
    }
    size_t sceneEntityCount = 0;
    bool readSkeletons = true;
    double lastReadMs = 0;
    bool discoveryBusy() const {
        return discoveryFuture.valid();
    }
    bool requestDiscovery() {
        if (!snapshotMode || discoveryFuture.valid())
            return false;
        discoveryEpoch = epoch;
        recoveryTick = GetTickCount64();
        const auto expectedPid = snapshotPid;
        const auto expectedBase = snapshotBase;
        const auto expectedVtable = entityVtable;
        discoveryFuture = std::async(std::launch::async, [expectedPid, expectedBase, expectedVtable] {
            GameReader worker;
            if (worker.entityVtable != expectedVtable)
                return SceneDiscovery{};
            worker.snapshotPid = expectedPid;
            worker.snapshotBase = expectedBase;
            if (!worker.attach() || worker.pid != expectedPid || worker.base != expectedBase)
                return SceneDiscovery{};
            worker.discoverSnapshotObjects();
            return SceneDiscovery{worker.snapshotEntities, worker.sceneWorlds, worker.cameraCandidates};
        });
        return true;
    }
    bool isSnapshotMode() const {
        return snapshotMode;
    }
    size_t discoverSnapshotObjects() {
        if (!snapshotMode || (!process && !attach()))
            return 0;
        if (pid != snapshotPid || base != snapshotBase) {
            status = "Sessao diferente do dump; redescoberta cancelada";
            return 0;
        }
        std::vector<uintptr_t> found, worlds, cameras;
        std::vector<unsigned char> buffer(1024 * 1024);
        uintptr_t cursor = 0;
        MEMORY_BASIC_INFORMATION info{};
        while (VirtualQueryEx(process, reinterpret_cast<LPCVOID>(cursor), &info, sizeof(info)) ==
               sizeof(info)) {
            const auto start = reinterpret_cast<uintptr_t>(info.BaseAddress);
            const auto end = start + info.RegionSize;
            if (end <= cursor)
                break;
            if (info.State == MEM_COMMIT && info.Type == MEM_PRIVATE &&
                !(info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) {
                for (uintptr_t at = start; at < end;) {
                    const SIZE_T requested = (std::min)(SIZE_T(end - at), buffer.size());
                    SIZE_T copied = 0;
                    if (ReadProcessMemory(process, reinterpret_cast<LPCVOID>(at), buffer.data(), requested,
                                          &copied)) {
                        for (SIZE_T i = 0; i + sizeof(uintptr_t) <= copied; i += sizeof(uintptr_t)) {
                            uintptr_t value{};
                            memcpy(&value, buffer.data() + i, sizeof(value));
                            if (value == base + cameraVtable) {
                                cameras.push_back(at + i);
                                continue;
                            }
                            if (value == base + offsets::engine::WorldVtableRva) {
                                worlds.push_back(at + i);
                                continue;
                            }
                            if (value != base + entityVtable)
                                continue;
                            const uintptr_t object = at + i;
                            uintptr_t world{}, array{}, skeleton{};
                            uint8_t slot{};
                            Vec3 xyz{};
                            if (!read(object + offsets::engine::EntityWorld, world) || !world ||
                                !read(object + offsets::engine::SkeletonSlot, slot) || !(slot & 128) ||
                                !read(object + offsets::engine::EntityComponents, array) ||
                                !read(array + (slot & 127) * 8, skeleton) || !skeleton ||
                                !read(object + offsets::engine::WorldPosition, xyz))
                                continue;
                            if (std::isfinite(xyz.x) && std::isfinite(xyz.y) && std::isfinite(xyz.z) &&
                                std::abs(xyz.x) < 100000 && std::abs(xyz.y) < 100000 &&
                                std::abs(xyz.z) < 100000)
                                found.push_back(object);
                        }
                    }
                    at += requested;
                }
            }
            cursor = end;
        }
        snapshotEntities = std::move(found);
        sceneWorlds = std::move(worlds);
        cameraCandidates = std::move(cameras);
        status = "Redescoberta: " + std::to_string(snapshotEntities.size()) +
                 " candidatos ativos. Classificacao experimental.";
        return snapshotEntities.size();
    }
    std::vector<std::pair<std::string, uintptr_t>> providedOffsets;
    std::string providedStatus = "Valores carregados; modulo de origem ainda nao confirmado";
    std::string status = "Layout nao validado: configure layout.ini";
    float matrix[16]{};
    std::vector<Entity> entities;
    int localTeam = 0;
    bool localTeamValid = false;
    bool trainingMode = false;
    size_t knownTeamEntities = 0;
    DWORD processId() const {
        return pid;
    }
    GameReader() {
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        ini = path;
        ini = ini.substr(0, ini.find_last_of(L"\\/") + 1) + L"layout.ini";
        reload();
    }
    ~GameReader() {
        disconnect();
    }
    void disconnect() {
        if (process)
            CloseHandle(process);
        process = nullptr;
        pid = 0;
        base = 0;
        moduleSize = 0;
        resetSceneState();
    }
    void reload() {
        disconnect();
        ++epoch;
        snapshotScanned = false;
        sceneWorlds.clear();
        cameraCandidates.clear();
        bootstrapEntities.clear();
        rigClasses.clear();
        aimRigIndices.clear();
        sceneRefreshTick = 0;
        snapshotMode = GetPrivateProfileIntW(L"messiah_snapshot", L"enabled", 0, ini.c_str()) == 1;
        snapshotPid = static_cast<DWORD>(offset(L"pid", L"messiah_snapshot"));
        snapshotBase = offset(L"module_base", L"messiah_snapshot");
        cameraAddress = offset(L"camera", L"messiah_snapshot");
        cameraVtable = offset(L"camera_vtable_rva", L"messiah_snapshot");
        entityVtable = offset(L"entity_vtable_rva", L"messiah_snapshot");
        snapshotEntities.clear();
        const auto snapshotCount = (std::min)(offset(L"count", L"messiah_snapshot"), uintptr_t(4096));
        for (uintptr_t i = 0; i < snapshotCount; ++i) {
            const auto key = L"entity_" + std::to_wstring(i);
            const auto address = offset(key.c_str(), L"messiah_snapshot");
            if (address >= 0x10000)
                snapshotEntities.push_back(address);
        }
        providedOffsets.clear();
        const wchar_t *keys[] = {L"il2cpp.gethandle",
                                 L"basenetworkable.typeinfo",
                                 L"basenetworkable.staticfields",
                                 L"basenetworkable.cliententities",
                                 L"basenetworkable.entitylist",
                                 L"basenetworkable.buffer",
                                 L"basenetworkable.entListBase",
                                 L"basenetworkable.entLS",
                                 L"maincamera.typeinfo",
                                 L"maincamera.staticfields"};
        for (const auto *key : keys) {
            std::wstring wide(key);
            std::string narrow;
            for (wchar_t ch : wide)
                narrow.push_back(static_cast<char>(ch)); // ASCII keys above.
            providedOffsets.emplace_back(narrow, offset(key, L"provided_offsets"));
        }
        providedStatus = "Valores carregados; clique em Verificar offsets no modulo";
        enabled = GetPrivateProfileIntW(L"layout", L"validated", 0, ini.c_str()) == 1;
        matrixRva = offset(L"matrix_rva");
        listRva = offset(L"entities_rva");
        countRva = offset(L"count_rva");
        localRva = offset(L"local_player_rva");
        feetOff = offset(L"feet_offset");
        headOff = offset(L"head_offset");
        healthOff = offset(L"health_offset");
        teamOff = offset(L"team_offset");
        maxHealthOff = offset(L"max_health_offset");
        if (!enabled || !matrixRva || !listRva || !countRva || !localRva) {
            enabled = false;
            status = "Layout nao validado: configure layout.ini";
        } else
            status = "Layout configurado; aguardando leitura valida";
        if (snapshotMode)
            status = "Integracao experimental com a sessao do dump; aguardando leitura";
    }
    bool attach() {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            status = "Falha ao listar processos";
            return false;
        }
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry))
            do {
                if (_wcsicmp(entry.szExeFile, L"BloodStrike.exe") == 0) {
                    pid = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        CloseHandle(snapshot);
        if (!pid) {
            status = "BloodStrike.exe nao encontrado";
            return false;
        }
        process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!process) {
            const DWORD error = GetLastError();
            status = "Leitura bloqueada pelo Windows (erro " + std::to_string(error) +
                     "). Sem dados para ESP/aimbot.";
            pid = 0;
            return false;
        }
        snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        MODULEENTRY32W module{};
        module.dwSize = sizeof(module);
        if (snapshot != INVALID_HANDLE_VALUE) {
            if (Module32FirstW(snapshot, &module))
                do {
                    if (_wcsicmp(module.szModule, L"BloodStrike.exe") == 0) {
                        base = reinterpret_cast<uintptr_t>(module.modBaseAddr);
                        moduleSize = module.modBaseSize;
                        break;
                    }
                } while (Module32NextW(snapshot, &module));
            CloseHandle(snapshot);
        }
        if (!base) {
            disconnect();
            status = "Modulo do jogo indisponivel";
            return false;
        }
        if (snapshotMode && !bindCurrentSession()) {
            const auto error = status;
            disconnect();
            status = error;
            return false;
        }
        status = enabled ? "Acesso permitido; aguardando dados do layout"
                         : "Acesso permitido, mas layout incompleto: ESP/aimbot indisponiveis";
        return true;
    }
    void probeProvided() {
        if (!process && !attach()) {
            providedStatus = status;
            return;
        }
        std::string result;
        const size_t roots[] = {0, 1, 8};
        for (const auto index : roots) {
            const auto &item = providedOffsets[index];
            result += item.first + ": ";
            if (!item.second || moduleSize < sizeof(uintptr_t) ||
                item.second > moduleSize - sizeof(uintptr_t)) {
                result += "fora da imagem de BloodStrike.exe\n";
                continue;
            }
            if (index == 0) {
                // A function RVA is not a pointer slot: never call unvalidated code.
                result += "dentro da imagem; funcao nao validada\n";
                continue;
            }
            uintptr_t value{};
            if (!read(base + item.second, value))
                result += "falha de leitura\n";
            else {
                char buffer[96];
                sprintf_s(buffer, "conteudo 0x%llX (tipo nao validado)\n",
                          static_cast<unsigned long long>(value));
                result += buffer;
            }
        }
        providedStatus = result;
    }
    bool update() {
        const auto readStart = std::chrono::steady_clock::now();
        struct ReadTimer {
            double &result;
            std::chrono::steady_clock::time_point start;
            ~ReadTimer() {
                result = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                             .count();
            }
        } timer{lastReadMs, readStart};
        pollDiscovery();
        entities.clear();
        localTeamValid = false;
        if (snapshotMode)
            return updateSnapshot();
        if (!enabled)
            return false;
        if (!process && !attach())
            return false;
        DWORD exitCode{};
        if (!GetExitCodeProcess(process, &exitCode) || exitCode != STILL_ACTIVE) {
            disconnect();
            status = "Processo encerrado";
            return false;
        }
        uintptr_t array{}, local{};
        int count{};
        if (!read(base + matrixRva, matrix) || !read(base + listRva, array) ||
            !read(base + countRva, count) || !read(base + localRva, local) || count < 0 || count > 2048 ||
            array < 0x10000 || local < 0x10000) {
            status = "Leitura invalida: confira o layout";
            return false;
        }
        float norm = 0;
        for (float v : matrix) {
            if (!std::isfinite(v)) {
                status = "Matriz invalida";
                return false;
            }
            norm += std::abs(v);
        }
        if (norm < 0.001f) {
            status = "Matriz vazia";
            return false;
        }
        localTeamValid = read(local + teamOff, localTeam);
        if (!localTeamValid) {
            status = "Jogador local invalido";
            return false;
        }
        for (int i = 0; i < count; ++i) {
            uintptr_t ptr{};
            Entity e{};
            if (!read(array + sizeof(uintptr_t) * i, ptr) || ptr == local)
                continue;
            if (!read(ptr + feetOff, e.feet) || !read(ptr + headOff, e.head) ||
                !read(ptr + healthOff, e.health) || !read(ptr + teamOff, e.team))
                continue;
            if (!std::isfinite(e.health) || e.health < 0 || e.health > 100000)
                continue;
            e.life = e.health > 0 ? LifeState::Alive : LifeState::Dead;
            e.healthKnown = true;
            e.teamKnown = true;
            if (maxHealthOff) {
                float maximum{};
                if (read(ptr + maxHealthOff, maximum) && std::isfinite(maximum) && maximum > 0 &&
                    maximum <= 100000)
                    e.maxHealth = maximum;
            }
            e.id = ptr;
            entities.push_back(e);
        }
        status = "Leitura concluida (compatibilidade depende do layout)";
        return true;
    }
};
