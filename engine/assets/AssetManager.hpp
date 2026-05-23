#pragma once

#include <engine/core/UUID.hpp>
#include <engine/renderer/Texture.hpp>
#include <engine/assets/AssetMetadata.hpp>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

namespace PixelEngine {

    class AssetManager {
    public:
        static void Init(VulkanContext& context);
        static void Shutdown();

        // Registry management
        static void ScanAssets(const std::string& directoryPath = "assets");
        static void SaveRegistry();
        static void LoadRegistry();

        // Assets loading/fetching
        static UUID LoadTexture(const std::string& path);
        static void ReloadTextureAsset(UUID handle, const std::string& path);
        
        static std::shared_ptr<Texture> GetTexture(UUID handle);
        static VkDescriptorSet GetTextureDescriptorSet(UUID handle);

        static bool HasAsset(UUID handle);
        static std::string GetAssetPath(UUID handle);
        static AssetType GetAssetType(UUID handle);
        
        // Getter for all registered assets (for ImGui UI)
        static const std::unordered_map<UUID, AssetMetadata>& GetMetadataRegistry() { return s_MetadataRegistry; }

    private:
        static void ScanDirectory(const std::filesystem::path& dirPath);
        static void ImportFile(const std::filesystem::path& filePath);

    private:
        struct TextureAsset {
            std::shared_ptr<Texture> Texture;
            VkDescriptorSet DescriptorSet;
        };

        static inline VulkanContext* s_Context = nullptr;
        static inline std::string s_AssetsRoot;

        // Registry database
        static inline std::unordered_map<UUID, AssetMetadata> s_MetadataRegistry;
        static inline std::unordered_map<std::string, UUID> s_PathToUUID;

        // Loaded runtime assets
        static inline std::unordered_map<UUID, TextureAsset> s_Textures;
    };

}
