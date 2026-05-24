#pragma once

#include <engine/core/UUID.hpp>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <array>
#include <map>
#include <vector>

#include <engine/renderer/Material.hpp>
#include <string>

namespace PixelEngine {

    struct IDComponent {
        UUID ID;

        IDComponent() = default;
        IDComponent(const IDComponent&) = default;
    };

    struct TagComponent {
        std::string Tag;

        TagComponent() = default;
        TagComponent(const TagComponent&) = default;
        TagComponent(const std::string& tag) : Tag(tag) {}
    };

    struct TransformComponent {
        glm::vec3 Translation = {0.0f, 0.0f, 0.0f};
        glm::vec3 Rotation = {0.0f, 0.0f, 0.0f};
        glm::vec3 Scale = {1.0f, 1.0f, 1.0f};

        TransformComponent() = default;
        TransformComponent(const TransformComponent&) = default;
        TransformComponent(const glm::vec3& translation) : Translation(translation) {}

        glm::mat4 GetTransform() const {
            glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));
            return glm::translate(glm::mat4(1.0f), Translation) * rotation * glm::scale(glm::mat4(1.0f), Scale);
        }
    };

    struct SpriteRendererComponent {
        Material Mat;
        
        SpriteRendererComponent() = default;
        SpriteRendererComponent(const SpriteRendererComponent&) = default;
        SpriteRendererComponent(const glm::vec4& color) { Mat.Color = color; }
    };

    struct MeshRendererComponent {
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
        UUID TextureID = 0;
        
        MeshRendererComponent() = default;
        MeshRendererComponent(const MeshRendererComponent&) = default;
        MeshRendererComponent(const glm::vec4& color) : Color(color) {}
    };

    struct HierarchyComponent {
        UUID Parent = 0;
        std::vector<UUID> Children;

        HierarchyComponent() = default;
        HierarchyComponent(const HierarchyComponent&) = default;
        HierarchyComponent(UUID parent) : Parent(parent) {}
    };

    struct VelocityComponent {
        glm::vec3 Linear{0.0f, 0.0f, 0.0f};
        glm::vec3 Angular{0.0f, 0.0f, 0.0f};

        VelocityComponent() = default;
        VelocityComponent(const VelocityComponent&) = default;
        VelocityComponent(const glm::vec3& linear) : Linear(linear) {}
    };

    struct SpriteAnimationComponent {
        std::vector<UUID> Textures;
        float FrameTime = 0.1f;
        int CurrentFrame = 0;
        float Timer = 0.0f;
        bool Loop = true;
        bool Playing = true;

        SpriteAnimationComponent() = default;
        SpriteAnimationComponent(const SpriteAnimationComponent&) = default;
    };

    struct TilemapTile {
        uint32_t TileIndex = 0;
    };

    struct TilemapChunk {
        static constexpr int ChunkSize = 16;
        std::array<TilemapTile, ChunkSize * ChunkSize> Tiles;

        TilemapChunk() {
            Tiles.fill(TilemapTile{0});
        }
    };

    struct TilemapComponent {
        UUID TilesetID = 0;
        uint32_t TileSize = 16;
        int RenderLayer = 0;

        std::map<std::pair<int, int>, TilemapChunk> Chunks;

        TilemapComponent() = default;
        TilemapComponent(const TilemapComponent&) = default;
    };

    struct AnimationFrame {
        std::string FrameName;
        std::string EventName;
    };

    struct AnimationClip {
        std::string Name;
        std::vector<AnimationFrame> Frames;
        float FPS = 10.0f;
        bool Loop = true;
    };

    struct AnimatorComponent {
        UUID SpriteSheetID = 0;
        std::string CurrentClip = "";
        int CurrentFrame = 0;
        float Timer = 0.0f;
        bool Playing = true;

        std::vector<AnimationClip> Clips;

        AnimatorComponent() = default;
        AnimatorComponent(const AnimatorComponent&) = default;
    };

    struct AudioSourceComponent {
        UUID ClipID = 0;
        bool Loop = false;
        bool PlayOnStart = false;
        float Volume = 1.0f;
        bool IsMusic = false;

        // Runtime state (ignored in serialization)
        SDL_AudioStream* Stream = nullptr;
        bool IsPlaying = false;

        AudioSourceComponent() = default;
        AudioSourceComponent(const AudioSourceComponent&) = default;
    };

}
