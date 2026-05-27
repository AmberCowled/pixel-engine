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

    nlohmann::json SceneSerializer::SerializeToJson() {
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
                spriteJson["Enabled"] = sc.Enabled;
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
                meshJson["Enabled"] = mc.Enabled;
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
                velocityJson["Enabled"] = vc.Enabled;
                velocityJson["Linear"] = SerializeVec3(vc.Linear);
                velocityJson["Angular"] = SerializeVec3(vc.Angular);
                entityJson["VelocityComponent"] = velocityJson;
            }

            // 8. Sprite Animation Component
            if (entity.HasComponent<SpriteAnimationComponent>()) {
                auto& ac = entity.GetComponent<SpriteAnimationComponent>();
                json animJson;
                animJson["Enabled"] = ac.Enabled;
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

            // 9. Tilemap Component
            if (entity.HasComponent<TilemapComponent>()) {
                auto& tc = entity.GetComponent<TilemapComponent>();
                json tmJson;
                tmJson["Enabled"] = tc.Enabled;
                tmJson["TilesetID"] = static_cast<uint64_t>(tc.TilesetID);
                tmJson["TileSize"] = tc.TileSize;
                tmJson["RenderLayer"] = tc.RenderLayer;
                
                json chunksJson = json::array();
                for (const auto& [coords, chunk] : tc.Chunks) {
                    json chunkJson;
                    chunkJson["x"] = coords.first;
                    chunkJson["y"] = coords.second;
                    
                    json tilesJson = json::array();
                    for (const auto& tile : chunk.Tiles) {
                        tilesJson.push_back(tile.TileIndex);
                    }
                    chunkJson["Tiles"] = tilesJson;
                    chunksJson.push_back(chunkJson);
                }
                tmJson["Chunks"] = chunksJson;
                entityJson["TilemapComponent"] = tmJson;
            }

            // 10. Animator Component
            if (entity.HasComponent<AnimatorComponent>()) {
                auto& ac = entity.GetComponent<AnimatorComponent>();
                json animJson;
                animJson["Enabled"] = ac.Enabled;
                animJson["SpriteSheetID"] = static_cast<uint64_t>(ac.SpriteSheetID);
                animJson["CurrentClip"] = ac.CurrentClip;
                animJson["CurrentFrame"] = ac.CurrentFrame;
                animJson["Playing"] = ac.Playing;

                json clipsJson = json::array();
                for (const auto& clip : ac.Clips) {
                    json clipJson;
                    clipJson["Name"] = clip.Name;
                    clipJson["FPS"] = clip.FPS;
                    clipJson["Loop"] = clip.Loop;

                    json framesJson = json::array();
                    for (const auto& frame : clip.Frames) {
                        json frameJson;
                        frameJson["FrameName"] = frame.FrameName;
                        frameJson["EventName"] = frame.EventName;
                        framesJson.push_back(frameJson);
                    }
                    clipJson["Frames"] = framesJson;
                    clipsJson.push_back(clipJson);
                }
                animJson["Clips"] = clipsJson;
                entityJson["AnimatorComponent"] = animJson;
            }

            // 11. Audio Source Component
            if (entity.HasComponent<AudioSourceComponent>()) {
                auto& asc = entity.GetComponent<AudioSourceComponent>();
                json audioJson;
                audioJson["Enabled"] = asc.Enabled;
                audioJson["ClipID"] = static_cast<uint64_t>(asc.ClipID);
                audioJson["Loop"] = asc.Loop;
                audioJson["PlayOnStart"] = asc.PlayOnStart;
                audioJson["Volume"] = asc.Volume;
                audioJson["IsMusic"] = asc.IsMusic;
                entityJson["AudioSourceComponent"] = audioJson;
            }

            // 12. Prefab Component
            if (entity.HasComponent<PrefabComponent>()) {
                auto& pc = entity.GetComponent<PrefabComponent>();
                json prefabJson;
                prefabJson["PrefabID"] = static_cast<uint64_t>(pc.PrefabID);
                prefabJson["OriginalUUID"] = static_cast<uint64_t>(pc.OriginalUUID);
                json overridesJson = json::array();
                for (const auto& field : pc.OverriddenFields) {
                    overridesJson.push_back(field);
                }
                prefabJson["OverriddenFields"] = overridesJson;
                entityJson["PrefabComponent"] = prefabJson;
            }

            // 13. Script Component
            if (entity.HasComponent<ScriptComponent>()) {
                auto& sc = entity.GetComponent<ScriptComponent>();
                json scriptJson;
                scriptJson["Enabled"] = sc.Enabled;
                scriptJson["ClassName"] = sc.ClassName;
                entityJson["ScriptComponent"] = scriptJson;
            }

            entitiesJson.push_back(entityJson);
        }

        outJson["Entities"] = entitiesJson;
        return outJson;
    }

    void SceneSerializer::Serialize(const std::string& filepath) {
        json outJson = SerializeToJson();

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

    bool SceneSerializer::DeserializeFromJson(const nlohmann::json& sceneJson) {
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
                sc.Enabled = scJson.value("Enabled", true);
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
                mc.Enabled = mcJson.value("Enabled", true);
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
                vc.Enabled = vcJson.value("Enabled", true);
                vc.Linear = DeserializeVec3(vcJson["Linear"]);
                vc.Angular = DeserializeVec3(vcJson["Angular"]);
            }

            // 6. Sprite Animation Component
            if (entityJson.find("SpriteAnimationComponent") != entityJson.end()) {
                auto& ac = entity.AddComponent<SpriteAnimationComponent>();
                auto& acJson = entityJson["SpriteAnimationComponent"];
                ac.Enabled = acJson.value("Enabled", true);
                for (auto& frameVal : acJson["Textures"]) {
                    ac.Textures.push_back(UUID(frameVal.get<uint64_t>()));
                }
                ac.FrameTime = acJson.value("FrameTime", 0.1f);
                ac.Loop = acJson.value("Loop", true);
                ac.Playing = acJson.value("Playing", true);
            }

            // 7. Tilemap Component
            if (entityJson.find("TilemapComponent") != entityJson.end()) {
                auto& tc = entity.AddComponent<TilemapComponent>();
                auto& tmJson = entityJson["TilemapComponent"];
                tc.Enabled = tmJson.value("Enabled", true);
                tc.TilesetID = UUID(tmJson.value("TilesetID", 0ull));
                tc.TileSize = tmJson.value("TileSize", 16u);
                tc.RenderLayer = tmJson.value("RenderLayer", 0);
                
                if (tmJson.contains("Chunks") && tmJson["Chunks"].is_array()) {
                    for (auto& chunkJson : tmJson["Chunks"]) {
                        int cx = chunkJson.value("x", 0);
                        int cy = chunkJson.value("y", 0);
                        
                        TilemapChunk chunk;
                        if (chunkJson.contains("Tiles") && chunkJson["Tiles"].is_array()) {
                            int idx = 0;
                            for (auto& tileVal : chunkJson["Tiles"]) {
                                if (idx < TilemapChunk::ChunkSize * TilemapChunk::ChunkSize) {
                                    chunk.Tiles[idx].TileIndex = tileVal.get<uint32_t>();
                                    idx++;
                                }
                            }
                        }
                        tc.Chunks[{cx, cy}] = chunk;
                    }
                }
            }

            // 8. Animator Component
            if (entityJson.find("AnimatorComponent") != entityJson.end()) {
                auto& ac = entity.AddComponent<AnimatorComponent>();
                auto& acJson = entityJson["AnimatorComponent"];
                ac.Enabled = acJson.value("Enabled", true);
                ac.SpriteSheetID = UUID(acJson.value("SpriteSheetID", 0ull));
                ac.CurrentClip = acJson.value("CurrentClip", "");
                ac.CurrentFrame = acJson.value("CurrentFrame", 0);
                ac.Playing = acJson.value("Playing", true);

                if (acJson.contains("Clips") && acJson["Clips"].is_array()) {
                    for (auto& clipJson : acJson["Clips"]) {
                        AnimationClip clip;
                        clip.Name = clipJson.value("Name", "");
                        clip.FPS = clipJson.value("FPS", 10.0f);
                        clip.Loop = clipJson.value("Loop", true);

                        if (clipJson.contains("Frames") && clipJson["Frames"].is_array()) {
                            for (auto& frameJson : clipJson["Frames"]) {
                                AnimationFrame frame;
                                frame.FrameName = frameJson.value("FrameName", "");
                                frame.EventName = frameJson.value("EventName", "");
                                clip.Frames.push_back(frame);
                            }
                        }
                        ac.Clips.push_back(clip);
                    }
                }
            }

            // 9. Audio Source Component
            if (entityJson.find("AudioSourceComponent") != entityJson.end()) {
                auto& asc = entity.AddComponent<AudioSourceComponent>();
                auto& audioJson = entityJson["AudioSourceComponent"];
                asc.Enabled = audioJson.value("Enabled", true);
                asc.ClipID = UUID(audioJson.value("ClipID", 0ull));
                asc.Loop = audioJson.value("Loop", false);
                asc.PlayOnStart = audioJson.value("PlayOnStart", false);
                asc.Volume = audioJson.value("Volume", 1.0f);
                asc.IsMusic = audioJson.value("IsMusic", false);
                asc.Stream = nullptr;
                asc.IsPlaying = false;
            }

            // 10. Prefab Component
            if (entityJson.find("PrefabComponent") != entityJson.end()) {
                auto& pc = entity.AddComponent<PrefabComponent>();
                auto& pcJson = entityJson["PrefabComponent"];
                pc.PrefabID = UUID(pcJson.value("PrefabID", 0ull));
                pc.OriginalUUID = UUID(pcJson.value("OriginalUUID", 0ull));
                if (pcJson.contains("OverriddenFields") && pcJson["OverriddenFields"].is_array()) {
                    for (auto& fieldVal : pcJson["OverriddenFields"]) {
                        pc.OverriddenFields.push_back(fieldVal.get<std::string>());
                    }
                }
            }

            // 11. Script Component
            if (entityJson.find("ScriptComponent") != entityJson.end()) {
                auto& sc = entity.AddComponent<ScriptComponent>();
                auto& scJson = entityJson["ScriptComponent"];
                sc.Enabled = scJson.value("Enabled", true);
                sc.ClassName = scJson.value("ClassName", "");
            }
        }
        return true;
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
        bool success = DeserializeFromJson(sceneJson);
        if (success) {
            PX_CORE_INFO("Scene deserialized successfully. Total entities: {0}", m_Scene.GetEntityMap().size());
        }
        return success;
    }

}
