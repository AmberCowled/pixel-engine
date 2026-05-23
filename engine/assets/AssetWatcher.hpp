#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>

namespace PixelEngine {

    class AssetWatcher {
    public:
        static void Init(const std::string& assetsDir);
        static void Update();

    private:
        static void ReimportAsset(const std::filesystem::path& path);

    private:
        static inline std::string s_AssetsDir;
        static inline std::unordered_map<std::string, std::filesystem::file_time_type> s_AssetFileTimestamps;
        static inline uint64_t s_LastCheckTime = 0;
    };

}
