#include "Scene.hpp"
#include "Entity.hpp"
#include "Components.hpp"
#include "SceneSerializer.hpp"
#include <engine/assets/AssetManager.hpp>
#include <engine/base/Log.hpp>
#include <engine/audio/AudioManager.hpp>

namespace PixelEngine {

    static bool CheckTilemapCollisionAxis(Scene* scene, const glm::vec3& position, const glm::vec3& scale, bool checkX, float& outPush) {
        auto tilemapView = scene->Reg().view<TransformComponent, TilemapComponent>();
        
        float entLeft = position.x - scale.x * 0.5f;
        float entRight = position.x + scale.x * 0.5f;
        float entBottom = position.y - scale.y * 0.5f;
        float entTop = position.y + scale.y * 0.5f;

        for (auto tmEntity : tilemapView) {
            auto& tmTransform = tilemapView.get<TransformComponent>(tmEntity);
            auto& tilemap = tilemapView.get<TilemapComponent>(tmEntity);
            auto tileset = AssetManager::GetTileset(tilemap.TilesetID);
            if (!tileset) continue;

            glm::vec3 tmPos = tmTransform.Translation;

            for (const auto& [coords, chunk] : tilemap.Chunks) {
                int cx = coords.first;
                int cy = coords.second;

                for (int ty = 0; ty < TilemapChunk::ChunkSize; ty++) {
                    for (int tx = 0; tx < TilemapChunk::ChunkSize; tx++) {
                        int idx = ty * TilemapChunk::ChunkSize + tx;
                        uint32_t tileIndex = chunk.Tiles[idx].TileIndex;
                        if (tileIndex == 0) continue;

                        if (tileset->SolidTiles.find(tileIndex) == tileset->SolidTiles.end() || !tileset->SolidTiles[tileIndex]) {
                            continue;
                        }

                        float tileWorldX = tmPos.x + static_cast<float>(cx * TilemapChunk::ChunkSize + tx);
                        float tileWorldY = tmPos.y + static_cast<float>(cy * TilemapChunk::ChunkSize + ty);

                        float tileLeft = tileWorldX;
                        float tileRight = tileWorldX + 1.0f;
                        float tileBottom = tileWorldY;
                        float tileTop = tileWorldY + 1.0f;

                        if (entRight > tileLeft && entLeft < tileRight && entTop > tileBottom && entBottom < tileTop) {
                            if (checkX) {
                                if (position.x < (tileLeft + tileRight) * 0.5f) {
                                    outPush = tileLeft - entRight; // Push left
                                } else {
                                    outPush = tileRight - entLeft; // Push right
                                }
                            } else {
                                if (position.y < (tileBottom + tileTop) * 0.5f) {
                                    outPush = tileBottom - entTop; // Push down
                                } else {
                                    outPush = tileTop - entBottom; // Push up
                                }
                            }
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }

    Scene::Scene() {}

    Scene::~Scene() {
        StopAllAudio();
    }

    std::shared_ptr<Scene> Scene::Clone(std::shared_ptr<Scene> source) {
        std::shared_ptr<Scene> dest = std::make_shared<Scene>();
        if (!source) return dest;

        SceneSerializer srcSerializer(*source);
        nlohmann::json data = srcSerializer.SerializeToJson();

        SceneSerializer destSerializer(*dest);
        destSerializer.DeserializeFromJson(data);

        return dest;
    }

    Entity Scene::CreateEntity(const std::string& name) {
        return CreateEntityWithUUID(UUID(), name);
    }

    Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name) {
        Entity entity = {m_Registry.create(), this};
        entity.AddComponent<IDComponent>();
        entity.GetComponent<IDComponent>().ID = uuid;
        entity.AddComponent<TransformComponent>();
        auto& tag = entity.AddComponent<TagComponent>();
        tag.Tag = name.empty() ? "Entity" : name;

        m_EntityMap[uuid] = entity;
        return entity;
    }

    void Scene::DestroyEntity(Entity entity) {
        if (entity.HasComponent<IDComponent>()) {
            m_EntityMap.erase(entity.GetComponent<IDComponent>().ID);
        }
        m_Registry.destroy(entity);
    }

    void Scene::OnUpdate(float deltaTime) {
        // 1. Physics / Movement System
        auto physicsView = m_Registry.view<TransformComponent, VelocityComponent>();
        for (auto entity : physicsView) {
            auto& transform = physicsView.get<TransformComponent>(entity);
            auto& velocity = physicsView.get<VelocityComponent>(entity);

            // Move X
            transform.Translation.x += velocity.Linear.x * deltaTime;
            float pushX = 0.0f;
            if (CheckTilemapCollisionAxis(this, transform.Translation, transform.Scale, true, pushX)) {
                transform.Translation.x += pushX;
                velocity.Linear.x = 0.0f;
            }

            // Move Y
            transform.Translation.y += velocity.Linear.y * deltaTime;
            float pushY = 0.0f;
            if (CheckTilemapCollisionAxis(this, transform.Translation, transform.Scale, false, pushY)) {
                transform.Translation.y += pushY;
                velocity.Linear.y = 0.0f;
            }

            transform.Rotation += velocity.Angular * deltaTime;
        }

        // 2. Sprite Animation System
        auto animView = m_Registry.view<SpriteRendererComponent, SpriteAnimationComponent>();
        for (auto entity : animView) {
            auto& sprite = animView.get<SpriteRendererComponent>(entity);
            auto& anim = animView.get<SpriteAnimationComponent>(entity);

            if (!anim.Playing || anim.Textures.empty()) continue;

            anim.Timer += deltaTime;
            if (anim.Timer >= anim.FrameTime) {
                anim.Timer -= anim.FrameTime;
                anim.CurrentFrame = anim.CurrentFrame + 1;
                if (anim.CurrentFrame >= static_cast<int>(anim.Textures.size())) {
                    if (anim.Loop) {
                        anim.CurrentFrame = 0;
                    } else {
                        anim.CurrentFrame = static_cast<int>(anim.Textures.size()) - 1;
                        anim.Playing = false;
                    }
                }
                sprite.Mat.TextureID = anim.Textures[anim.CurrentFrame];
            }
        }

        // 3. Animator System
        auto animatorView = m_Registry.view<SpriteRendererComponent, AnimatorComponent>();
        for (auto entity : animatorView) {
            auto& sprite = animatorView.get<SpriteRendererComponent>(entity);
            auto& animator = animatorView.get<AnimatorComponent>(entity);

            if (!animator.Playing || animator.CurrentClip.empty()) continue;

            const AnimationClip* clip = nullptr;
            for (const auto& c : animator.Clips) {
                if (c.Name == animator.CurrentClip) {
                    clip = &c;
                    break;
                }
            }
            if (!clip || clip->Frames.empty()) continue;

            float frameTime = 1.0f / clip->FPS;
            animator.Timer += deltaTime;
            if (animator.Timer >= frameTime) {
                animator.Timer -= frameTime;
                animator.CurrentFrame++;
                if (animator.CurrentFrame >= static_cast<int>(clip->Frames.size())) {
                    if (clip->Loop) {
                        animator.CurrentFrame = 0;
                    } else {
                        animator.CurrentFrame = static_cast<int>(clip->Frames.size()) - 1;
                        animator.Playing = false;
                    }
                }

                const auto& frame = clip->Frames[animator.CurrentFrame];
                
                if (!frame.EventName.empty()) {
                    PX_CORE_INFO("Animation Event triggered on entity {0}: {1}", static_cast<uint32_t>(entity), frame.EventName);
                }

                auto spritesheet = AssetManager::GetSpriteSheet(animator.SpriteSheetID);
                if (spritesheet) {
                    sprite.Mat.TextureID = spritesheet->TextureID;
                    auto fit = spritesheet->Frames.find(frame.FrameName);
                    if (fit != spritesheet->Frames.end()) {
                        sprite.Mat.UVs = fit->second.UVs;
                    }
                }
            }
        }

        // 4. Audio Source System
        if (!m_AudioInitialized) {
            auto audioView = m_Registry.view<AudioSourceComponent>();
            for (auto entity : audioView) {
                auto& audio = audioView.get<AudioSourceComponent>(entity);
                if (audio.PlayOnStart) {
                    audio.IsPlaying = true;
                }
            }
            m_AudioInitialized = true;
        }

        auto audioView = m_Registry.view<AudioSourceComponent>();
        for (auto entity : audioView) {
            auto& audio = audioView.get<AudioSourceComponent>(entity);
            if (audio.IsPlaying) {
                if (!audio.Stream) {
                    auto clip = AssetManager::GetAudioClip(audio.ClipID);
                    if (clip && clip->Buffer) {
                        audio.Stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &clip->Spec, nullptr, nullptr);
                        if (audio.Stream) {
                            SDL_PutAudioStreamData(audio.Stream, clip->Buffer, clip->Length);
                            SDL_ResumeAudioStreamDevice(audio.Stream);
                        }
                    }
                }

                if (audio.Stream) {
                    float master = AudioManager::GetMasterVolume();
                    float bus = audio.IsMusic ? AudioManager::GetMusicVolume() : AudioManager::GetSFXVolume();
                    SDL_SetAudioStreamGain(audio.Stream, audio.Volume * bus * master);

                    // Check if finished playing
                    if (SDL_GetAudioStreamQueued(audio.Stream) == 0) {
                        if (audio.Loop) {
                            auto clip = AssetManager::GetAudioClip(audio.ClipID);
                            if (clip && clip->Buffer) {
                                SDL_PutAudioStreamData(audio.Stream, clip->Buffer, clip->Length);
                            }
                        } else {
                            SDL_DestroyAudioStream(audio.Stream);
                            audio.Stream = nullptr;
                            audio.IsPlaying = false;
                        }
                    }
                }
            } else {
                if (audio.Stream) {
                    SDL_DestroyAudioStream(audio.Stream);
                    audio.Stream = nullptr;
                }
            }
        }
    }

    glm::mat4 Scene::GetWorldTransform(Entity entity) {
        if (!entity) return glm::mat4(1.0f);

        glm::mat4 transform = glm::mat4(1.0f);
        if (entity.HasComponent<TransformComponent>()) {
            transform = entity.GetComponent<TransformComponent>().GetTransform();
        }

        if (entity.HasComponent<HierarchyComponent>()) {
            auto& hc = entity.GetComponent<HierarchyComponent>();
            if (hc.Parent != 0) {
                auto parentEntity = GetEntityByUUID(hc.Parent);
                if (parentEntity) {
                    return GetWorldTransform(parentEntity) * transform;
                }
            }
        }

        return transform;
    }

    Entity Scene::GetEntityByUUID(UUID uuid) {
        auto it = m_EntityMap.find(uuid);
        if (it != m_EntityMap.end()) {
            return {it->second, this};
        }
        return {};
    }

    Entity::Entity(entt::entity handle, Scene* scene) : m_EntityHandle(handle), m_Scene(scene) {}

    void Scene::StopAllAudio() {
        auto audioView = m_Registry.view<AudioSourceComponent>();
        for (auto entity : audioView) {
            auto& audio = audioView.get<AudioSourceComponent>(entity);
            if (audio.Stream) {
                SDL_DestroyAudioStream(audio.Stream);
                audio.Stream = nullptr;
            }
            audio.IsPlaying = false;
        }
    }

}
