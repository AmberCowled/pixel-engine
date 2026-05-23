#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>

namespace PixelEngine {

    class ShaderHotReloader {
    public:
        static void Init(const std::string& shaderDir);
        static void Update();

    private:
        static void RecompileShader(const std::filesystem::path& path);

    private:
        static inline std::string s_ShaderDir;
        static inline std::unordered_map<std::string, std::filesystem::file_time_type> s_ShaderFileTimestamps;
        static inline uint64_t s_LastCheckTime = 0;
    };

}
