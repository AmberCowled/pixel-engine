#pragma once

#include <engine/core/UUID.hpp>
#include <engine/renderer/Texture.hpp>
#include <engine/assets/AssetMetadata.hpp>
#include <engine/assets/Tileset.hpp>
#include <engine/assets/SpriteSheet.hpp>
#include <engine/assets/AudioClip.hpp>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

namespace PixelEngine {

    class AssetManager {
    public:
        static void Init(VulkanContext& context, const std::string& directoryPath = "assets");
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

        static std::shared_ptr<Tileset> GetTileset(UUID handle);
        static std::shared_ptr<SpriteSheet> GetSpriteSheet(UUID handle);
        static std::shared_ptr<AudioClip> GetAudioClip(UUID handle);

        static UUID LoadTileset(const std::string& path);
        static UUID LoadSpriteSheet(const std::string& path);
        static UUID LoadAudioClip(const std::string& path);

        static bool HasAsset(UUID handle);
        static std::string GetAssetPath(UUID handle);
        static AssetType GetAssetType(UUID handle);
        
        // Getter for all registered assets (for ImGui UI)
        static const std::unordered_map<UUID, AssetMetadata>& GetMetadataRegistry() { return s_MetadataRegistry; }

        static size_t GetLoadedTexturesCount() { return s_Textures.size(); }
        static size_t GetLoadedTilesetsCount() { return s_Tilesets.size(); }
        static size_t GetLoadedSpriteSheetsCount() { return s_SpriteSheets.size(); }
        static size_t GetLoadedAudioClipsCount() { return s_AudioClips.size(); }

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
        static inline std::unordered_map<UUID, std::shared_ptr<Tileset>> s_Tilesets;
        static inline std::unordered_map<UUID, std::shared_ptr<SpriteSheet>> s_SpriteSheets;
        static inline std::unordered_map<UUID, std::shared_ptr<AudioClip>> s_AudioClips;
    };

}
