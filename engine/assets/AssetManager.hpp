#pragma once

#include <engine/core/UUID.hpp>
#include <engine/renderer/Texture.hpp>
#include <unordered_map>
#include <memory>
#include <string>

namespace PixelEngine {

    class AssetManager {
    public:
        static void Init(VulkanContext& context);
        static void Shutdown();

        static UUID LoadTexture(const std::string& path);
        static std::shared_ptr<Texture> GetTexture(UUID handle);
        static VkDescriptorSet GetTextureDescriptorSet(UUID handle);

        static bool HasAsset(UUID handle);

    private:
        struct TextureAsset {
            std::shared_ptr<Texture> Texture;
            VkDescriptorSet DescriptorSet;
        };

        static inline VulkanContext* s_Context = nullptr;
        static inline std::unordered_map<UUID, TextureAsset> s_Textures;
        static inline std::unordered_map<std::string, UUID> s_PathToUUID;
    };

}
