#pragma once

#include <engine/ecs/Entity.hpp>
#include <engine/ecs/Components.hpp>
#include <engine/core/UUID.hpp>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace PixelEngine {

    inline bool IsPartOfPrefab(Entity entity, Scene* scene, UUID& outPrefabID) {
        if (!entity || !scene) return false;
        if (entity.HasComponent<PrefabComponent>()) {
            outPrefabID = entity.GetComponent<PrefabComponent>().PrefabID;
            return true;
        }
        if (entity.HasComponent<HierarchyComponent>()) {
            auto parentUUID = entity.GetComponent<HierarchyComponent>().Parent;
            if (parentUUID != 0) {
                auto parentEnt = scene->GetEntityByUUID(parentUUID);
                return IsPartOfPrefab(parentEnt, scene, outPrefabID);
            }
        }
        return false;
    }

    inline void TrackOverride(Entity entity, const std::string& fieldName) {
        if (!entity || !entity.HasComponent<PrefabComponent>()) return;
        auto& pc = entity.GetComponent<PrefabComponent>();
        if (std::find(pc.OverriddenFields.begin(), pc.OverriddenFields.end(), fieldName) == pc.OverriddenFields.end()) {
            pc.OverriddenFields.push_back(fieldName);
        }
    }

    inline std::filesystem::path FindPrefabPath(UUID prefabID, bool projectLoaded, const std::string& projectPath) {
        std::filesystem::path assetsPath = projectLoaded ? (std::filesystem::path(projectPath) / "assets") : std::filesystem::path("assets");
        if (std::filesystem::exists(assetsPath)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsPath)) {
                if (entry.is_regular_file() && (entry.path().extension() == ".json" && entry.path().string().find(".prefab") != std::string::npos)) {
                    std::ifstream fin(entry.path());
                    if (fin.is_open()) {
                        try {
                            nlohmann::json j;
                            fin >> j;
                            if (j.contains("PrefabID") && j["PrefabID"].get<uint64_t>() == static_cast<uint64_t>(prefabID)) {
                                return entry.path();
                            }
                        } catch (...) {}
                    }
                }
            }
        }
        return {};
    }

}
