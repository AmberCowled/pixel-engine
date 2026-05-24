#include "AssetManager.hpp"
#include <engine/renderer/VulkanContext.hpp>
#include <engine/base/Log.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>

using json = nlohmann::json;

namespace PixelEngine {

    void AssetManager::Init(VulkanContext& context, const std::string& directoryPath) {
        s_Context = &context;
        
        // Clear previous runtime databases for dynamic project swapping
        s_Textures.clear();
        s_Tilesets.clear();
        s_SpriteSheets.clear();
        s_AudioClips.clear();
        s_MetadataRegistry.clear();
        s_PathToUUID.clear();
        
        s_AssetsRoot = std::filesystem::absolute(directoryPath).string();
        LoadRegistry();
        ScanAssets(directoryPath);
        SaveRegistry();
    }

    void AssetManager::Shutdown() {
        s_Textures.clear();
        s_Tilesets.clear();
        s_SpriteSheets.clear();
        s_AudioClips.clear();
        s_MetadataRegistry.clear();
        s_PathToUUID.clear();
        s_Context = nullptr;
        PX_CORE_INFO("AssetManager shutdown.");
    }

    static std::string AssetTypeToString(AssetType type) {
        switch (type) {
            case AssetType::Texture:     return "Texture";
            case AssetType::Audio:       return "Audio";
            case AssetType::Shader:      return "Shader";
            case AssetType::Scene:       return "Scene";
            case AssetType::Tileset:     return "Tileset";
            case AssetType::SpriteSheet: return "SpriteSheet";
            default:                     return "None";
        }
    }

    static AssetType StringToAssetType(const std::string& str) {
        if (str == "Texture")     return AssetType::Texture;
        if (str == "Audio")       return AssetType::Audio;
        if (str == "Shader")      return AssetType::Shader;
        if (str == "Scene")       return AssetType::Scene;
        if (str == "Tileset")     return AssetType::Tileset;
        if (str == "SpriteSheet") return AssetType::SpriteSheet;
        return AssetType::None;
    }

    static AssetType DetectAssetType(const std::filesystem::path& path) {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") return AssetType::Texture;
        if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") return AssetType::Audio;
        if (ext == ".vert" || ext == ".frag" || ext == ".spv") return AssetType::Shader;
        if (ext == ".json") return AssetType::Scene;
        if (ext == ".tileset") return AssetType::Tileset;
        if (ext == ".spritesheet") return AssetType::SpriteSheet;
        return AssetType::None;
    }

    void AssetManager::ScanAssets(const std::string& directoryPath) {
        s_AssetsRoot = "";

        std::vector<std::string> searchPaths = {
            directoryPath,
            "assets",
            "../assets",
            "../../assets"
        };

        for (const auto& path : searchPaths) {
            if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
                s_AssetsRoot = std::filesystem::absolute(path).string();
                break;
            }
        }

        if (s_AssetsRoot.empty()) {
            PX_CORE_WARN("AssetManager: Could not find assets root folder '{0}'!", directoryPath);
            return;
        }

        PX_CORE_INFO("AssetManager: Scanning assets root: {0}", s_AssetsRoot);
        ScanDirectory(s_AssetsRoot);
    }

    void AssetManager::ScanDirectory(const std::filesystem::path& dirPath) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                auto path = entry.path();
                // Exclude meta files and registry itself
                if (path.extension() != ".meta" && path.filename() != "asset_registry.json") {
                    ImportFile(path);
                }
            }
        }
    }

    void AssetManager::ImportFile(const std::filesystem::path& filePath) {
        std::string absolutePath = std::filesystem::absolute(filePath).string();
        std::filesystem::path relativePath = std::filesystem::relative(filePath, s_AssetsRoot);
        
        std::filesystem::path metaPath = filePath.string() + ".meta";
        UUID uuid;
        AssetType type = DetectAssetType(filePath);

        if (type == AssetType::None) return; // Skip unsupported files

        if (std::filesystem::exists(metaPath)) {
            // Read existing meta file
            std::ifstream fin(metaPath);
            if (fin.is_open()) {
                try {
                    json metaJson;
                    fin >> metaJson;
                    uuid = UUID(metaJson["uuid"].get<uint64_t>());
                    type = StringToAssetType(metaJson.value("type", AssetTypeToString(type)));
                } catch (...) {
                    PX_CORE_WARN("AssetManager: Failed to parse meta file: {0}, regenerating...", metaPath.string());
                    uuid = UUID(); // regenerate
                }
                fin.close();
            }
        } else {
            // Create new meta file
            uuid = UUID();
            json metaJson = {
                {"uuid", static_cast<uint64_t>(uuid)},
                {"type", AssetTypeToString(type)}
            };
            std::ofstream fout(metaPath);
            if (fout.is_open()) {
                fout << std::setw(4) << metaJson << std::endl;
                fout.close();
            }
        }

        // Register asset
        AssetMetadata meta;
        meta.ID = uuid;
        meta.Type = type;
        meta.SourcePath = relativePath.string();

        s_MetadataRegistry[uuid] = meta;
        s_PathToUUID[absolutePath] = uuid;
    }

    void AssetManager::SaveRegistry() {
        if (s_AssetsRoot.empty()) return;

        json registryJson = json::array();
        for (const auto& [uuid, meta] : s_MetadataRegistry) {
            json item = {
                {"uuid", static_cast<uint64_t>(uuid)},
                {"path", meta.SourcePath},
                {"type", AssetTypeToString(meta.Type)}
            };
            registryJson.push_back(item);
        }

        std::string registryPath = (std::filesystem::path(s_AssetsRoot) / "asset_registry.json").string();
        std::ofstream fout(registryPath);
        if (fout.is_open()) {
            fout << std::setw(4) << registryJson << std::endl;
            fout.close();
        }
    }

    void AssetManager::LoadRegistry() {
        std::string root = s_AssetsRoot;
        if (root.empty()) {
            // Find assets root first
            std::vector<std::string> searchPaths = { "assets", "../assets", "../../assets" };
            for (const auto& path : searchPaths) {
                if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
                    root = std::filesystem::absolute(path).string();
                    break;
                }
            }
        }

        if (root.empty()) return;

        std::string registryPath = (std::filesystem::path(root) / "asset_registry.json").string();
        if (!std::filesystem::exists(registryPath)) return;

        std::ifstream fin(registryPath);
        if (fin.is_open()) {
            try {
                json registryJson;
                fin >> registryJson;
                for (const auto& item : registryJson) {
                    uint64_t uuidVal = item["uuid"].get<uint64_t>();
                    UUID uuid(uuidVal);
                    std::string relativePath = item["path"].get<std::string>();
                    std::string typeStr = item["type"].get<std::string>();

                    AssetMetadata meta;
                    meta.ID = uuid;
                    meta.Type = StringToAssetType(typeStr);
                    meta.SourcePath = relativePath;

                    s_MetadataRegistry[uuid] = meta;
                    
                    std::string absPath = std::filesystem::absolute(std::filesystem::path(root) / relativePath).string();
                    s_PathToUUID[absPath] = uuid;
                }
                PX_CORE_INFO("AssetManager: Preloaded {0} assets from registry database.", s_MetadataRegistry.size());
            } catch (...) {
                PX_CORE_ERROR("AssetManager: Failed to parse asset registry file: {0}", registryPath);
            }
            fin.close();
        }
    }

    UUID AssetManager::LoadTexture(const std::string& path) {
        if (!s_Context) {
            PX_CORE_ERROR("AssetManager not initialized!");
            return 0;
        }

        std::string absolutePath = std::filesystem::absolute(path).string();
        UUID handle;

        // Check if path map already exists
        if (s_PathToUUID.find(absolutePath) != s_PathToUUID.end()) {
            handle = s_PathToUUID[absolutePath];
        } else {
            // Import file dynamically
            ImportFile(path);
            if (s_PathToUUID.find(absolutePath) != s_PathToUUID.end()) {
                handle = s_PathToUUID[absolutePath];
                SaveRegistry();
            } else {
                return 0;
            }
        }

        // Check if runtime texture is already loaded
        if (s_Textures.find(handle) != s_Textures.end()) {
            return handle;
        }

        try {
            auto texture = std::make_shared<Texture>(*s_Context, path);
            VkDescriptorSet descriptorSet = s_Context->CreateTextureDescriptorSet(texture->GetImageView());
            
            TextureAsset asset;
            asset.Texture = texture;
            asset.DescriptorSet = descriptorSet;

            s_Textures[handle] = asset;
            return handle;
        } catch (...) {
            PX_CORE_ERROR("Failed to load texture: {0}", path);
            return 0;
        }
    }

    void AssetManager::ReloadTextureAsset(UUID handle, const std::string& path) {
        if (s_Textures.find(handle) != s_Textures.end()) {
            PX_CORE_INFO("AssetManager: Reloading texture asset {0} in-place...", handle);
            s_Textures[handle].Texture->UpdateData(path);
        }
    }

    std::shared_ptr<Texture> AssetManager::GetTexture(UUID handle) {
        auto it = s_Textures.find(handle);
        if (it != s_Textures.end()) {
            return it->second.Texture;
        }
        return nullptr;
    }

    VkDescriptorSet AssetManager::GetTextureDescriptorSet(UUID handle) {
        auto it = s_Textures.find(handle);
        if (it != s_Textures.end()) {
            return it->second.DescriptorSet;
        }
        return VK_NULL_HANDLE;
    }

    bool AssetManager::HasAsset(UUID handle) {
        return s_MetadataRegistry.find(handle) != s_MetadataRegistry.end();
    }

    std::string AssetManager::GetAssetPath(UUID handle) {
        auto it = s_MetadataRegistry.find(handle);
        if (it != s_MetadataRegistry.end()) {
            // Find absolute path
            return (std::filesystem::path(s_AssetsRoot) / it->second.SourcePath).string();
        }
        return "";
    }

    AssetType AssetManager::GetAssetType(UUID handle) {
        auto it = s_MetadataRegistry.find(handle);
        if (it != s_MetadataRegistry.end()) {
            return it->second.Type;
        }
        return AssetType::None;
    }

    std::shared_ptr<Tileset> AssetManager::GetTileset(UUID handle) {
        auto it = s_Tilesets.find(handle);
        if (it != s_Tilesets.end()) {
            return it->second;
        }

        std::string path = GetAssetPath(handle);
        if (!path.empty() && GetAssetType(handle) == AssetType::Tileset) {
            LoadTileset(path);
            auto it2 = s_Tilesets.find(handle);
            if (it2 != s_Tilesets.end()) {
                return it2->second;
            }
        }
        return nullptr;
    }

    std::shared_ptr<SpriteSheet> AssetManager::GetSpriteSheet(UUID handle) {
        auto it = s_SpriteSheets.find(handle);
        if (it != s_SpriteSheets.end()) {
            return it->second;
        }

        std::string path = GetAssetPath(handle);
        if (!path.empty() && GetAssetType(handle) == AssetType::SpriteSheet) {
            LoadSpriteSheet(path);
            auto it2 = s_SpriteSheets.find(handle);
            if (it2 != s_SpriteSheets.end()) {
                return it2->second;
            }
        }
        return nullptr;
    }

    UUID AssetManager::LoadTileset(const std::string& path) {
        std::string absolutePath = std::filesystem::absolute(path).string();
        UUID handle = 0;

        auto it = s_PathToUUID.find(absolutePath);
        if (it != s_PathToUUID.end()) {
            handle = it->second;
        } else {
            handle = UUID();
            s_PathToUUID[absolutePath] = handle;

            AssetMetadata meta;
            meta.ID = handle;
            meta.Type = AssetType::Tileset;
            meta.SourcePath = std::filesystem::relative(path, s_AssetsRoot).string();
            s_MetadataRegistry[handle] = meta;
        }

        if (s_Tilesets.find(handle) != s_Tilesets.end()) {
            return handle;
        }

        std::ifstream fin(path);
        if (!fin.is_open()) {
            PX_CORE_ERROR("Failed to open tileset file: {0}", path);
            return 0;
        }

        try {
            json tsJson;
            fin >> tsJson;

            auto tileset = std::make_shared<Tileset>();
            tileset->ID = handle;
            tileset->TileSize = tsJson.value("tileSize", 16u);

            std::string relativeTexPath = tsJson.value("texturePath", "");
            if (!relativeTexPath.empty()) {
                std::filesystem::path texAbsPath = std::filesystem::path(s_AssetsRoot) / relativeTexPath;
                tileset->TextureID = LoadTexture(texAbsPath.string());
            }

            if (tsJson.contains("solidTiles") && tsJson["solidTiles"].is_array()) {
                for (auto& item : tsJson["solidTiles"]) {
                    if (item.is_number()) {
                        tileset->SolidTiles[item.get<uint32_t>()] = true;
                    }
                }
            }

            s_Tilesets[handle] = tileset;
        } catch (const std::exception& e) {
            PX_CORE_ERROR("Failed to parse tileset JSON {0}: {1}", path, e.what());
            return 0;
        }

        return handle;
    }

    UUID AssetManager::LoadSpriteSheet(const std::string& path) {
        std::string absolutePath = std::filesystem::absolute(path).string();
        UUID handle = 0;

        auto it = s_PathToUUID.find(absolutePath);
        if (it != s_PathToUUID.end()) {
            handle = it->second;
        } else {
            handle = UUID();
            s_PathToUUID[absolutePath] = handle;

            AssetMetadata meta;
            meta.ID = handle;
            meta.Type = AssetType::SpriteSheet;
            meta.SourcePath = std::filesystem::relative(path, s_AssetsRoot).string();
            s_MetadataRegistry[handle] = meta;
        }

        if (s_SpriteSheets.find(handle) != s_SpriteSheets.end()) {
            return handle;
        }

        std::ifstream fin(path);
        if (!fin.is_open()) {
            PX_CORE_ERROR("Failed to open spritesheet file: {0}", path);
            return 0;
        }

        try {
            json ssJson;
            fin >> ssJson;

            auto spritesheet = std::make_shared<SpriteSheet>();
            spritesheet->ID = handle;

            std::string relativeTexPath = ssJson.value("texturePath", "");
            if (!relativeTexPath.empty()) {
                std::filesystem::path texAbsPath = std::filesystem::path(s_AssetsRoot) / relativeTexPath;
                spritesheet->TextureID = LoadTexture(texAbsPath.string());
            }

            auto tex = GetTexture(spritesheet->TextureID);
            float texWidth = tex ? (float)tex->GetWidth() : 1.0f;
            float texHeight = tex ? (float)tex->GetHeight() : 1.0f;

            if (ssJson.contains("frames") && ssJson["frames"].is_object()) {
                for (auto& [frameName, frameData] : ssJson["frames"].items()) {
                    float fx = frameData.value("x", 0.0f);
                    float fy = frameData.value("y", 0.0f);
                    float fw = frameData.value("w", 0.0f);
                    float fh = frameData.value("h", 0.0f);

                    SpriteSheetFrame frame;
                    float uMin = fx / texWidth;
                    float uMax = (fx + fw) / texWidth;
                    float vMin = fy / texHeight;
                    float vMax = (fy + fh) / texHeight;

                    frame.UVs = {
                        glm::vec2{ uMin, vMin },
                        glm::vec2{ uMax, vMin },
                        glm::vec2{ uMax, vMax },
                        glm::vec2{ uMin, vMax }
                    };

                    spritesheet->Frames[frameName] = frame;
                }
            }

            s_SpriteSheets[handle] = spritesheet;
        } catch (const std::exception& e) {
            PX_CORE_ERROR("Failed to parse spritesheet JSON {0}: {1}", path, e.what());
            return 0;
        }

        return handle;
    }

    std::shared_ptr<AudioClip> AssetManager::GetAudioClip(UUID handle) {
        auto it = s_AudioClips.find(handle);
        if (it != s_AudioClips.end()) {
            return it->second;
        }

        std::string path = GetAssetPath(handle);
        if (!path.empty() && GetAssetType(handle) == AssetType::Audio) {
            LoadAudioClip(path);
            auto it2 = s_AudioClips.find(handle);
            if (it2 != s_AudioClips.end()) {
                return it2->second;
            }
        }
        return nullptr;
    }

    UUID AssetManager::LoadAudioClip(const std::string& path) {
        std::string absolutePath = std::filesystem::absolute(path).string();
        UUID handle = 0;

        auto it = s_PathToUUID.find(absolutePath);
        if (it != s_PathToUUID.end()) {
            handle = it->second;
        } else {
            handle = UUID();
            s_PathToUUID[absolutePath] = handle;

            AssetMetadata meta;
            meta.ID = handle;
            meta.Type = AssetType::Audio;
            meta.SourcePath = std::filesystem::relative(path, s_AssetsRoot).string();
            s_MetadataRegistry[handle] = meta;
        }

        if (s_AudioClips.find(handle) != s_AudioClips.end()) {
            return handle;
        }

        auto clip = std::make_shared<AudioClip>();
        clip->ID = handle;
        clip->Path = path;

        if (!SDL_LoadWAV(path.c_str(), &clip->Spec, &clip->Buffer, &clip->Length)) {
            PX_CORE_ERROR("AssetManager: Failed to load WAV file {0}: {1}", path, SDL_GetError());
            return 0;
        }

        s_AudioClips[handle] = clip;
        return handle;
    }

}
