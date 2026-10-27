#pragma once
#include "offsets.h"
#include <Windows.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Read-only adapter for the CPython 3.14 / Messiah build captured in this session.
// No Python evaluation, remote calls, writes or injected code.
class PythonProfiles {
  public:
    using Read = std::function<bool(uintptr_t, void *, size_t)>;
    struct Team {
        int faction = 0;
        uintptr_t avatar = 0;
        std::string name;
        bool healthKnown = false;
        float health = 0, maxHealth = 0;
        int combatState = -1;
    };
    std::unordered_map<uintptr_t, Team> entities;
    bool localValid = false;
    bool trainingMode = false;
    int localFaction = 0;
    uintptr_t localAvatar = 0;
    uintptr_t localNative = 0;
    std::string status = "Aguardando faccoes";

  private:
    Read read;
    uintptr_t moduleBase = 0, interpreter = 0, globalModule = 0, managerModule = 0;
    uint64_t tick = 0;
    std::unordered_map<std::string, uintptr_t> keys;
    struct PropertyIndex {
        uintptr_t index = 0;
        std::unordered_map<std::string, int> fields;
    };
    std::unordered_map<uintptr_t, PropertyIndex> propertyIndices;
    template <class T> bool get(uintptr_t at, T &value) const {
        return at >= 0x10000 && read && read(at, &value, sizeof(value));
    }
    uintptr_t ptr(uintptr_t at) const {
        uintptr_t value = 0;
        get(at, value);
        return value;
    }
    std::string type(uintptr_t obj) const {
        char name[96]{};
        auto at = ptr(ptr(obj + offsets::python::ObjectType) + offsets::python::TypeName);
        if (!get(at, name))
            return {};
        name[sizeof(name) - 1] = 0;
        return name;
    }
    std::string string(uintptr_t obj) const {
        if (type(obj) != "str")
            return {};
        unsigned char h[40]{};
        if (!get(obj, h))
            return {};
        uint64_t length{};
        uint32_t state{};
        std::memcpy(&length, h + offsets::python::UnicodeLength, 8);
        std::memcpy(&state, h + offsets::python::UnicodeState, 4);
        if (length > 192 || (state & 96) != 96)
            return {};
        std::string text(static_cast<size_t>(length), '\0');
        if (length && !read(obj + offsets::python::UnicodeAsciiData, text.data(), text.size()))
            return {};
        return text;
    }
    using Pair = std::pair<uintptr_t, uintptr_t>;
    std::vector<Pair> dictionary(uintptr_t obj, size_t limit = 8192) const {
        if (type(obj) != "dict")
            return {};
        unsigned char dh[48]{}, kh[32]{};
        if (!get(obj, dh))
            return {};
        uintptr_t kp{}, vp{};
        std::memcpy(&kp, dh + offsets::python::DictKeys, 8);
        std::memcpy(&vp, dh + offsets::python::DictValues, 8);
        if (!get(kp, kh))
            return {};
        const auto log = kh[9], kind = kh[10];
        uint64_t count{};
        std::memcpy(&count, kh + offsets::python::KeysEntryCount, 8);
        if (log > 24 || kind > 2 || count > limit)
            return {};
        const size_t stride = kind ? 16 : 24;
        std::vector<unsigned char> data(static_cast<size_t>(count) * stride);
        if (!data.empty() &&
            !read(kp + offsets::python::KeysHeaderSize + (uintptr_t(1) << log), data.data(), data.size()))
            return {};
        std::vector<uintptr_t> values;
        if (vp) {
            values.resize(static_cast<size_t>(count));
            if (count && !read(vp + offsets::python::SplitValuesHeader, values.data(), values.size() * 8))
                return {};
        }
        std::vector<Pair> out;
        out.reserve(static_cast<size_t>(count));
        for (size_t i = 0; i < count; ++i) {
            uintptr_t key{}, value{};
            std::memcpy(&key, data.data() + i * stride + (kind ? 0 : 8), 8);
            if (vp)
                value = values[i];
            else
                std::memcpy(&value, data.data() + i * stride + (kind ? 8 : 16), 8);
            if (key && value)
                out.emplace_back(key, value);
        }
        // A resize during the read invalidates this sample.
        if (ptr(obj + offsets::python::DictKeys) != kp || ptr(obj + offsets::python::DictValues) != vp)
            return {};
        return out;
    }
    uintptr_t lookup(uintptr_t dict, const std::string &name) {
        auto items = dictionary(dict);
        auto known = keys.find(name);
        if (known != keys.end())
            for (const auto &item : items)
                if (item.first == known->second && string(item.first) == name)
                    return item.second;
        for (const auto &item : items)
            if (string(item.first) == name) {
                keys[name] = item.first;
                return item.second;
            }
        return 0;
    }
    uintptr_t attributes(uintptr_t obj) const {
        const auto t = ptr(obj + offsets::python::ObjectType);
        int64_t offset = 0;
        if (!get(t + offsets::python::TypeDictOffset, offset))
            return 0;
        if (offset > 0 && offset <= 4096)
            return ptr(obj + static_cast<uintptr_t>(offset));
        uint32_t flags = 0;
        get(t + offsets::python::TypeFlags, flags);
        if (flags & 16)
            return ptr(obj - offsets::python::ManagedDictPreheader); // Py_TPFLAGS_MANAGED_DICT, GIL build
        return 0;
    }
    int propertyIndex(uintptr_t avatar, const std::string &name) {
        auto state = ptr(avatar + offsets::python::EntityState),
             mirror = ptr(state + offsets::python::StateMirror),
             t = ptr(mirror + offsets::python::ObjectType);
        auto descriptor = lookup(ptr(t + offsets::python::TypeDictionary), "__property_index__");
        auto index = ptr(descriptor + offsets::python::PropertyIndexData);
        if (!index)
            return -1;
        auto cached = propertyIndices.find(t);
        if (cached != propertyIndices.end() && cached->second.index == index) {
            const auto field = cached->second.fields.find(name);
            return field == cached->second.fields.end() ? -1 : field->second;
        }
        auto sentinel = ptr(index + offsets::python::IndexSentinel),
             node = ptr(sentinel + offsets::python::NodeNext);
        std::unordered_set<uintptr_t> seen;
        PropertyIndex found;
        found.index = index;
        while (node >= 0x10000 && node != sentinel && seen.size() < 2048 && seen.insert(node).second) {
            uintptr_t fields[4]{};
            if (!get(node, fields))
                break;
            const auto key = string(fields[2]);
            int value = -1;
            if (!key.empty() && get(fields[3], value) && value >= 0 && value < 2048)
                found.fields[key] = value;
            node = fields[1];
        }
        const auto field = found.fields.find(name);
        const int result = field == found.fields.end() ? -1 : field->second;
        if (node == sentinel)
            propertyIndices[t] = std::move(found);
        return result;
    }
    bool integer(uintptr_t avatar, const std::string &name, int &result) {
        const int index = propertyIndex(avatar, name);
        if (index < 0)
            return false;
        const auto store = ptr(ptr(avatar + offsets::python::EntityState) + offsets::python::StateStore),
                   record = ptr(store + offsets::python::StoreRecord),
                   container = ptr(record + offsets::python::RecordContainer);
        unsigned char value[16]{};
        if (!get(ptr(container + offsets::python::ContainerValues) +
                     static_cast<uintptr_t>(index) * offsets::python::ValueStride,
                 value) ||
            value[offsets::python::ValueKind] != 0)
            return false;
        int64_t number{};
        std::memcpy(&number, value, 8);
        if (number < -2147483647LL || number > 2147483647LL)
            return false;
        result = static_cast<int>(number);
        return true;
    }
    bool faction(uintptr_t avatar, int &result) {
        // Verified Squad Fight factions: 1 and 2. Neutral/unknown is not an enemy.
        return integer(avatar, "faction", result) && (result == 1 || result == 2);
    }
    bool number(uintptr_t avatar, const std::string &name, float &result) {
        const int index = propertyIndex(avatar, name);
        if (index < 0)
            return false;
        const auto store = ptr(ptr(avatar + offsets::python::EntityState) + offsets::python::StateStore),
                   container =
                       ptr(ptr(store + offsets::python::StoreRecord) + offsets::python::RecordContainer);
        unsigned char value[16]{};
        if (!get(ptr(container + offsets::python::ContainerValues) +
                     uintptr_t(index) * offsets::python::ValueStride,
                 value))
            return false;
        double n = 0;
        if (value[offsets::python::ValueKind] == 0) {
            int64_t i = 0;
            std::memcpy(&i, value, 8);
            n = static_cast<double>(i);
        } else if (value[offsets::python::ValueKind] == 1)
            std::memcpy(&n, value, 8);
        else
            return false;
        if (!std::isfinite(n) || n < 0 || n > 100000)
            return false;
        result = static_cast<float>(n);
        return true;
    }
    std::string textProperty(uintptr_t avatar, const std::string &field) {
        const int index = propertyIndex(avatar, field);
        if (index < 0)
            return {};
        const auto store = ptr(ptr(avatar + offsets::python::EntityState) + offsets::python::StateStore),
                   container =
                       ptr(ptr(store + offsets::python::StoreRecord) + offsets::python::RecordContainer);
        unsigned char value[16]{};
        if (!get(ptr(container + offsets::python::ContainerValues) +
                     uintptr_t(index) * offsets::python::ValueStride,
                 value) ||
            value[offsets::python::ValueKind] != 2)
            return {};
        uintptr_t text = 0;
        std::memcpy(&text, value, 8);
        uintptr_t length = 0, capacity = 0;
        if (!get(text + offsets::python::NativeStringLength, length) ||
            !get(text + offsets::python::NativeStringCapacity, capacity) || !length || length > 128 ||
            capacity < length)
            return {};
        const auto data = capacity > 15 ? ptr(text) : text;
        std::string result(static_cast<size_t>(length), '\0');
        if (!read(data, result.data(), result.size()))
            return {};
        return result;
    }
    std::string teamId(uintptr_t avatar) {
        auto id = textProperty(avatar, "combat_team_id");
        for (const auto c : id)
            if (static_cast<unsigned char>(c) < 33 || static_cast<unsigned char>(c) > 126)
                return {};
        return id;
    }
    void profile(Team &info) {
        info.name = textProperty(info.avatar, "show_name");
        if (info.name.empty())
            info.name = textProperty(info.avatar, "name");
        // Names are UTF-8 game strings. Reject malformed text and control codes.
        if (!info.name.empty()) {
            bool controls = false;
            for (const auto c : info.name)
                if (static_cast<unsigned char>(c) < 32 || c == 127)
                    controls = true;
            if (controls || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, info.name.data(),
                                                 static_cast<int>(info.name.size()), nullptr, 0))
                info.name.clear();
        }
        int state = -1;
        if (!integer(info.avatar, "inner_combat_state", state))
            integer(info.avatar, "combat_state", state);
        if (state >= 0 && state <= 5)
            info.combatState = state;
        info.healthKnown = number(info.avatar, "server_hp", info.health);
        // Matches cur_maxhp: DYING uses maxdhp, otherwise maxhp.
        if (info.combatState >= 0) {
            float maximum = 0;
            if (number(info.avatar, state == 1 ? "maxdhp" : "maxhp", maximum) && maximum > 0)
                info.maxHealth = maximum;
        }
    }
    uintptr_t nativeEntity(uintptr_t avatar, uintptr_t entityVtable) {
        const auto model = lookup(attributes(avatar), "model");
        const auto wrapper = lookup(attributes(model), "model");
        if (type(wrapper) != "IEntity")
            return 0;
        auto native = ptr(ptr(wrapper + offsets::python::WrapperHolder) + offsets::python::HolderNative);
        return ptr(native) == entityVtable ? native : 0;
    }

  public:
    void reset() {
        *this = PythonProfiles{};
    }
    void refresh(uintptr_t base, uintptr_t entityVtable, uint64_t now, Read reader) {
        if (moduleBase && moduleBase != base)
            reset();
        read = std::move(reader);
        moduleBase = base;
        if (tick && now - tick < 100)
            return;
        tick = now;
        localValid = false;
        trainingMode = false;
        localFaction = 0;
        localAvatar = 0;
        localNative = 0;
        entities.clear();
        const auto runtime = base + offsets::python::RuntimeRva;
        char cookie[8]{};
        uint64_t version = 0, gil = 1;
        if (!get(runtime, cookie) || std::memcmp(cookie, "xdebugpy", 8) ||
            !get(runtime + offsets::python::RuntimeVersion, version) ||
            version != offsets::python::ExpectedVersion || !get(runtime + offsets::python::RuntimeGil, gil) ||
            gil != 0) {
            status = "Runtime de faccoes incompativel";
            return;
        }
        const auto interp = ptr(runtime + offsets::python::RuntimeInterpreters);
        if (!interp) {
            status = "Interpretador indisponivel";
            return;
        }
        if (interpreter != interp) {
            interpreter = interp;
            globalModule = managerModule = 0;
            keys.clear();
            propertyIndices.clear();
        }
        if (type(globalModule) != "module" || type(managerModule) != "module") {
            const auto modules = ptr(interpreter + offsets::python::InterpreterModules);
            globalModule = lookup(modules, "GlobalData");
            managerModule = lookup(modules, "common.EntityManager");
        }
        auto local = lookup(ptr(globalModule + offsets::python::ModuleDictionary), "p");
        const auto localType = type(local);
        if (localType.find("CombatAvatar") == std::string::npos) {
            status = "Aguardando jogador de Squad Fight (" + localType + ")";
            return;
        }
        localNative = nativeEntity(local, entityVtable);
        if (localType == "PlayerCombatAvatarShootingRange") {
            trainingMode = true;
            localAvatar = local;
            status = "Campo de treino identificado";
            return;
        }
        int mode = 0;
        if (!integer(local, "match_type", mode) || mode <= 0) {
            status = "Modo de partida indisponivel";
            return;
        }
        const bool squadFight = mode == 1200;
        const auto localId = squadFight ? std::string{} : teamId(local);
        if ((squadFight && !faction(local, localFaction)) || (!squadFight && localId.empty())) {
            status =
                "Faccao local indisponivel (indice " + std::to_string(propertyIndex(local, "faction")) + ")";
            return;
        }
        if (!squadFight)
            localFaction = 1;
        const auto manager = lookup(ptr(managerModule + offsets::python::ModuleDictionary), "EntityManager");
        const auto registry = lookup(ptr(manager + offsets::python::TypeDictionary), "_entities");
        auto avatars = dictionary(registry, 16384);
        if (avatars.empty()) {
            status = "Registro de jogadores indisponivel";
            return;
        }
        std::unordered_map<std::string, int> teamIds;
        if (!squadFight)
            teamIds[localId] = 1;
        for (const auto &entry : avatars) {
            const auto avatar = entry.second;
            if (type(avatar).find("CombatAvatar") == std::string::npos)
                continue;
            int team = 0;
            if (squadFight) {
                if (!faction(avatar, team))
                    continue;
            } else {
                const auto id = teamId(avatar);
                if (id.empty())
                    continue;
                auto item = teamIds.find(id);
                if (item == teamIds.end())
                    item = teamIds.emplace(id, static_cast<int>(teamIds.size() + 1)).first;
                team = item->second;
            }
            const auto native = nativeEntity(avatar, entityVtable);
            if (native) {
                Team info;
                info.faction = team;
                info.avatar = avatar;
                profile(info);
                entities[native] = std::move(info);
            }
        }
        // Local player changed while reading: discard the complete sample.
        if (lookup(ptr(globalModule + offsets::python::ModuleDictionary), "p") != local) {
            entities.clear();
            status = "Troca de jogador";
            return;
        }
        localAvatar = local;
        localValid = true;
        status = squadFight ? "Faccoes de Squad Fight identificadas"
                            : "Equipes por combat_team_id (modo " + std::to_string(mode) + ")";
    }
};
