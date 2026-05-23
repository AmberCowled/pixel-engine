#include "AssetManager.hpp"
#include <engine/renderer/VulkanContext.hpp>
#include <engine/base/Log.hpp>
#include <filesystem>

namespace PixelEngine {

    void AssetManager::Init(VulkanContext& context) {
        s_Context = &context;
    }

    void AssetManager::Shutdown() {
        s_Textures.clear();
        s_PathToUUID.clear();
        s_Context = nullptr;
    }

    UUID AssetManager::LoadTexture(const std::string& path) {
        if (!s_Context) {
            PX_CORE_ERROR("AssetManager not initialized!");
            return 0;
        }

        // Check if already loaded
        std::string absolutePath = std::filesystem::absolute(path).string();
        if (s_PathToUUID.find(absolutePath) != s_PathToUUID.end()) {
            return s_PathToUUID[absolutePath];
        }

        try {
            UUID handle;
            auto texture = std::make_shared<Texture>(*s_Context, path);
            VkDescriptorSet descriptorSet = s_Context->CreateTextureDescriptorSet(texture->GetImageView());
            
            TextureAsset asset;
            asset.Texture = texture;
            asset.DescriptorSet = descriptorSet;

            s_Textures[handle] = asset;
            s_PathToUUID[absolutePath] = handle;
            return handle;
        } catch (...) {
            PX_CORE_ERROR("Failed to load texture: {0}", path);
            return 0;
        }
    }

    std::shared_ptr<Texture> AssetManager::GetTexture(UUID handle) {
        if (s_Textures.find(handle) != s_Textures.end()) {
            return s_Textures[handle].Texture;
        }
        return nullptr;
    }

    VkDescriptorSet AssetManager::GetTextureDescriptorSet(UUID handle) {
        if (s_Textures.find(handle) != s_Textures.end()) {
            return s_Textures[handle].DescriptorSet;
        }
        return VK_NULL_HANDLE;
    }

    bool AssetManager::HasAsset(UUID handle) {
        return s_Textures.find(handle) != s_Textures.end();
    }

}
