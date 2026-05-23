#include "SceneSerializer.hpp"
#include "Entity.hpp"
#include "Components.hpp"
#include <nlohmann/json.hpp>
#include <engine/base/Log.hpp>
#include <fstream>
#include <iomanip>

using json = nlohmann::json;

namespace PixelEngine {

    SceneSerializer::SceneSerializer(Scene& scene) : m_Scene(scene) {}

    // Helper functions for serializing glm types
    static json SerializeVec3(const glm::vec3& v) {
        return {v.x, v.y, v.z};
    }

    static json SerializeVec4(const glm::vec4& v) {
        return {v.x, v.y, v.z, v.w};
    }

    static glm::vec3 DeserializeVec3(const json& j) {
        return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
    }

    static glm::vec4 DeserializeVec4(const json& j) {
        return glm::vec4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
    }

    void SceneSerializer::Serialize(const std::string& filepath) {
        json outJson;
        outJson["SceneName"] = "Untitled Scene";

        json entitiesJson = json::array();

        auto view = m_Scene.Reg().view<IDComponent>();
        for (auto entityID : view) {
            Entity entity = {entityID, &m_Scene};
            if (!entity) continue;

            json entityJson;

            // 1. ID Component
            entityJson["UUID"] = static_cast<uint64_t>(entity.GetComponent<IDComponent>().ID);

            // 2. Tag Component
            if (entity.HasComponent<TagComponent>()) {
                entityJson["Tag"] = entity.GetComponent<TagComponent>().Tag;
            }

            // 3. Transform Component
            if (entity.HasComponent<TransformComponent>()) {
                auto& tc = entity.GetComponent<TransformComponent>();
                json transformJson;
                transformJson["Translation"] = SerializeVec3(tc.Translation);
                transformJson["Rotation"] = SerializeVec3(tc.Rotation);
                transformJson["Scale"] = SerializeVec3(tc.Scale);
                entityJson["TransformComponent"] = transformJson;
            }

            // 4. Sprite Renderer Component
            if (entity.HasComponent<SpriteRendererComponent>()) {
                auto& sc = entity.GetComponent<SpriteRendererComponent>();
                json spriteJson;
                spriteJson["Material"] = {
                    {"ShaderName", sc.Mat.ShaderName},
                    {"TextureID", static_cast<uint64_t>(sc.Mat.TextureID)},
                    {"Color", SerializeVec4(sc.Mat.Color)},
                    {"Blend", static_cast<int>(sc.Mat.Blend)}
                };
                entityJson["SpriteRendererComponent"] = spriteJson;
            }

            // 5. Mesh Renderer Component
            if (entity.HasComponent<MeshRendererComponent>()) {
                auto& mc = entity.GetComponent<MeshRendererComponent>();
                json meshJson;
                meshJson["Color"] = SerializeVec4(mc.Color);
                meshJson["TextureID"] = static_cast<uint64_t>(mc.TextureID);
                entityJson["MeshRendererComponent"] = meshJson;
            }

            // 6. Hierarchy Component
            if (entity.HasComponent<HierarchyComponent>()) {
                auto& hc = entity.GetComponent<HierarchyComponent>();
                json hierarchyJson;
                hierarchyJson["Parent"] = static_cast<uint64_t>(hc.Parent);
                json childrenJson = json::array();
                for (auto childUUID : hc.Children) {
                    childrenJson.push_back(static_cast<uint64_t>(childUUID));
                }
                hierarchyJson["Children"] = childrenJson;
                entityJson["HierarchyComponent"] = hierarchyJson;
            }

            // 7. Velocity Component
            if (entity.HasComponent<VelocityComponent>()) {
                auto& vc = entity.GetComponent<VelocityComponent>();
                json velocityJson;
                velocityJson["Linear"] = SerializeVec3(vc.Linear);
                velocityJson["Angular"] = SerializeVec3(vc.Angular);
                entityJson["VelocityComponent"] = velocityJson;
            }

            // 8. Sprite Animation Component
            if (entity.HasComponent<SpriteAnimationComponent>()) {
                auto& ac = entity.GetComponent<SpriteAnimationComponent>();
                json animJson;
                json framesJson = json::array();
                for (auto texID : ac.Textures) {
                    framesJson.push_back(static_cast<uint64_t>(texID));
                }
                animJson["Textures"] = framesJson;
                animJson["FrameTime"] = ac.FrameTime;
                animJson["Loop"] = ac.Loop;
                animJson["Playing"] = ac.Playing;
                entityJson["SpriteAnimationComponent"] = animJson;
            }

            entitiesJson.push_back(entityJson);
        }

        outJson["Entities"] = entitiesJson;

        std::filesystem::path path(filepath);
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream fout(filepath);
        if (fout.is_open()) {
            fout << std::setw(4) << outJson << std::endl;
            PX_CORE_INFO("Scene successfully serialized to {0}", filepath);
        } else {
            PX_CORE_ERROR("Failed to open file for scene serialization: {0}", filepath);
        }
    }

    bool SceneSerializer::Deserialize(const std::string& filepath) {
        std::ifstream fin(filepath);
        if (!fin.is_open()) {
            PX_CORE_WARN("Failed to open scene file for deserialization: {0}", filepath);
            return false;
        }

        json sceneJson;
        try {
            fin >> sceneJson;
        } catch (const std::exception& e) {
            PX_CORE_ERROR("Failed to parse scene JSON from file {0}: {1}", filepath, e.what());
            return false;
        }

        PX_CORE_INFO("Deserializing scene from: {0}", filepath);

        // Clear existing scene data
        m_Scene.Reg().clear();
        m_Scene.m_EntityMap.clear();

        if (sceneJson.find("Entities") == sceneJson.end()) return true;

        for (auto& entityJson : sceneJson["Entities"]) {
            uint64_t uuidVal = entityJson["UUID"].get<uint64_t>();
            UUID uuid(uuidVal);
            std::string name = entityJson.value("Tag", "Entity");

            Entity entity = m_Scene.CreateEntityWithUUID(uuid, name);

            // 1. Transform Component
            if (entityJson.find("TransformComponent") != entityJson.end()) {
                auto& tc = entity.GetComponent<TransformComponent>();
                auto& tcJson = entityJson["TransformComponent"];
                tc.Translation = DeserializeVec3(tcJson["Translation"]);
                tc.Rotation = DeserializeVec3(tcJson["Rotation"]);
                tc.Scale = DeserializeVec3(tcJson["Scale"]);
            }

            // 2. Sprite Renderer Component
            if (entityJson.find("SpriteRendererComponent") != entityJson.end()) {
                auto& sc = entity.AddComponent<SpriteRendererComponent>();
                auto& scJson = entityJson["SpriteRendererComponent"];
                auto& matJson = scJson["Material"];
                sc.Mat.ShaderName = matJson.value("ShaderName", "sprite");
                sc.Mat.TextureID = UUID(matJson["TextureID"].get<uint64_t>());
                sc.Mat.Color = DeserializeVec4(matJson["Color"]);
                sc.Mat.Blend = static_cast<BlendMode>(matJson.value("Blend", 1));
            }

            // 3. Mesh Renderer Component
            if (entityJson.find("MeshRendererComponent") != entityJson.end()) {
                auto& mc = entity.AddComponent<MeshRendererComponent>();
                auto& mcJson = entityJson["MeshRendererComponent"];
                mc.Color = DeserializeVec4(mcJson["Color"]);
                mc.TextureID = UUID(mcJson["TextureID"].get<uint64_t>());
            }

            // 4. Hierarchy Component
            if (entityJson.find("HierarchyComponent") != entityJson.end()) {
                auto& hc = entity.AddComponent<HierarchyComponent>();
                auto& hcJson = entityJson["HierarchyComponent"];
                hc.Parent = UUID(hcJson["Parent"].get<uint64_t>());
                for (auto& childVal : hcJson["Children"]) {
                    hc.Children.push_back(UUID(childVal.get<uint64_t>()));
                }
            }

            // 5. Velocity Component
            if (entityJson.find("VelocityComponent") != entityJson.end()) {
                auto& vc = entity.AddComponent<VelocityComponent>();
                auto& vcJson = entityJson["VelocityComponent"];
                vc.Linear = DeserializeVec3(vcJson["Linear"]);
                vc.Angular = DeserializeVec3(vcJson["Angular"]);
            }

            // 6. Sprite Animation Component
            if (entityJson.find("SpriteAnimationComponent") != entityJson.end()) {
                auto& ac = entity.AddComponent<SpriteAnimationComponent>();
                auto& acJson = entityJson["SpriteAnimationComponent"];
                for (auto& frameVal : acJson["Textures"]) {
                    ac.Textures.push_back(UUID(frameVal.get<uint64_t>()));
                }
                ac.FrameTime = acJson.value("FrameTime", 0.1f);
                ac.Loop = acJson.value("Loop", true);
                ac.Playing = acJson.value("Playing", true);
            }
        }

        PX_CORE_INFO("Scene deserialized successfully. Total entities: {0}", m_Scene.GetEntityMap().size());
        return true;
    }

}
