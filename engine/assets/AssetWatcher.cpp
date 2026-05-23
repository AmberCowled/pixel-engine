#include "AssetWatcher.hpp"
#include <engine/assets/AssetManager.hpp>
#include <engine/renderer/ShaderHotReloader.hpp>
#include <engine/base/Log.hpp>
#include <SDL3/SDL.h>
#include <vector>

namespace PixelEngine {

    void AssetWatcher::Init(const std::string& assetsDir) {
        s_AssetsDir = "";

        // Auto-detect assets directory
        std::vector<std::string> searchPaths = {
            assetsDir,
            "assets",
            "../assets",
            "../../assets",
            "../../../assets"
        };

        for (const auto& path : searchPaths) {
            if (path.empty()) continue;
            if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
                s_AssetsDir = std::filesystem::absolute(path).string();
                break;
            }
        }

        if (s_AssetsDir.empty()) {
            PX_CORE_WARN("AssetWatcher: Could not find assets directory. Hot reloading assets will be disabled.");
            return;
        }

        PX_CORE_INFO("AssetWatcher: Watching assets in directory: {0}", s_AssetsDir);

        // Record initial timestamps recursively
        for (const auto& entry : std::filesystem::recursive_directory_iterator(s_AssetsDir)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".json") {
                    s_AssetFileTimestamps[entry.path().string()] = std::filesystem::last_write_time(entry.path());
                }
            }
        }

        s_LastCheckTime = SDL_GetTicks();
    }

    void AssetWatcher::Update() {
        // Also update the shader hot reloader
        ShaderHotReloader::Update();

        if (s_AssetsDir.empty()) return;

        uint64_t currentTime = SDL_GetTicks();
        if (currentTime - s_LastCheckTime < 500) {
            return; // Check every 500ms
        }
        s_LastCheckTime = currentTime;

        // Scan recursively for changes
        for (const auto& entry : std::filesystem::recursive_directory_iterator(s_AssetsDir)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".json") {
                    std::string filePath = entry.path().string();
                    auto lastWrite = std::filesystem::last_write_time(entry.path());

                    // Check if file is new or modified
                    if (s_AssetFileTimestamps.find(filePath) == s_AssetFileTimestamps.end() ||
                        s_AssetFileTimestamps[filePath] < lastWrite) {
                        
                        s_AssetFileTimestamps[filePath] = lastWrite;
                        PX_CORE_INFO("AssetWatcher: Change detected in {0}, re-importing...", entry.path().filename().string());
                        
                        ReimportAsset(entry.path());
                    }
                }
            }
        }
    }

    void AssetWatcher::ReimportAsset(const std::filesystem::path& path) {
        std::string absolutePath = std::filesystem::absolute(path).string();
        
        // Find if this path has an existing mapping in the AssetManager
        // Since s_PathToUUID mapping is loaded in AssetManager registry, we can query AssetManager.
        // Wait, how do we access s_PathToUUID from here?
        // We can expose a static lookup method in AssetManager or query it directly if we add a getter,
        // or just let AssetManager handle reloading if we pass the path.
        // Wait, we can implement:
        // AssetManager::LoadTexture(path) will return the UUID. If it's already loaded, it will reload or we can trigger ReloadTextureAsset.
        // Let's see: we can look up the UUID dynamically!
        // To do this, let's look at AssetManager.hpp: s_PathToUUID is private.
        // But wait! We can add a simple public function in AssetManager:
        // `static UUID GetUUIDFromPath(const std::string& path)`
        // Or we can just add a lookup inside AssetWatcher if we make AssetManager a friend class,
        // or since s_PathToUUID is static inline, we can expose `GetUUIDFromPath`:
        // Let's check AssetManager.hpp. We can add:
        // `static UUID GetUUIDFromPath(const std::string& path);`
        // Let's implement this lookup inside AssetManager.cpp:
        // ```cpp
        // UUID AssetManager::GetUUIDFromPath(const std::string& path) {
        //     std::string absPath = std::filesystem::absolute(path).string();
        //     auto it = s_PathToUUID.find(absPath);
        //     if (it != s_PathToUUID.end()) return it->second;
        //     return 0;
        // }
        // ```
        // Yes, this is extremely clean!
        // Then in AssetWatcher::ReimportAsset:
        // ```cpp
        // UUID uuid = AssetManager::GetUUIDFromPath(absolutePath);
        // if (uuid != 0) {
        //     AssetType type = AssetManager::GetAssetType(uuid);
        //     if (type == AssetType::Texture) {
        //         AssetManager::ReloadTextureAsset(uuid, absolutePath);
        //     } else if (type == AssetType::Scene) {
        //         PX_CORE_INFO("AssetWatcher: Scene file '{0}' modified. You can reload it from the Engine Controls panel.", path.filename().string());
        //     }
        // }
        // ```
        // This is absolutely beautiful!
        
        // Let's add GetUUIDFromPath lookup declaration in AssetManager.hpp and definition in AssetManager.cpp first or alongside it.
        // Let's write the code here assuming we have GetUUIDFromPath.
        UUID uuid = AssetManager::LoadTexture(absolutePath); // LoadTexture returns existing UUID and loads it if new.
        if (uuid != 0) {
            AssetType type = AssetManager::GetAssetType(uuid);
            if (type == AssetType::Texture) {
                AssetManager::ReloadTextureAsset(uuid, absolutePath);
            } else if (type == AssetType::Scene) {
                PX_CORE_INFO("AssetWatcher: Scene file '{0}' modified on disk.", path.filename().string());
            }
        }
    }

}
