#include "ShaderHotReloader.hpp"
#include <engine/base/Log.hpp>
#include <engine/renderer/Renderer2D.hpp>
#include <SDL3/SDL.h>
#include <cstdlib>
#include <iostream>

namespace PixelEngine {

    void ShaderHotReloader::Init(const std::string& shaderDir) {
        s_ShaderDir = "";

        // Auto-detect shaders directory
        std::vector<std::string> searchPaths = {
            shaderDir,
            "shaders",
            "../shaders",
            "../../shaders",
            "../../../shaders"
        };

        for (const auto& path : searchPaths) {
            if (path.empty()) continue;
            if (std::filesystem::exists(path) && std::filesystem::exists(path + "/sprite.vert")) {
                s_ShaderDir = path;
                break;
            }
        }

        if (s_ShaderDir.empty()) {
            PX_CORE_WARN("ShaderHotReloader: Could not find shader source directory. Hot reloading will be disabled.");
            return;
        }

        PX_CORE_INFO("ShaderHotReloader: Watching shaders in directory: {0}", s_ShaderDir);

        // Record initial timestamps
        for (const auto& entry : std::filesystem::directory_iterator(s_ShaderDir)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                if (ext == ".vert" || ext == ".frag") {
                    s_ShaderFileTimestamps[entry.path().string()] = std::filesystem::last_write_time(entry.path());
                }
            }
        }

        s_LastCheckTime = SDL_GetTicks();
    }

    void ShaderHotReloader::Update() {
        if (s_ShaderDir.empty()) return;

        uint64_t currentTime = SDL_GetTicks();
        if (currentTime - s_LastCheckTime < 500) {
            return; // Only check every 500ms
        }
        s_LastCheckTime = currentTime;

        bool anyCompiled = false;

        // Check for modifications
        for (const auto& entry : std::filesystem::directory_iterator(s_ShaderDir)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                if (ext == ".vert" || ext == ".frag") {
                    std::string filePath = entry.path().string();
                    auto lastWrite = std::filesystem::last_write_time(entry.path());

                    // Check if file is new or modified
                    if (s_ShaderFileTimestamps.find(filePath) == s_ShaderFileTimestamps.end() ||
                        s_ShaderFileTimestamps[filePath] < lastWrite) {
                        
                        s_ShaderFileTimestamps[filePath] = lastWrite;
                        PX_CORE_INFO("ShaderHotReloader: Change detected in {0}, compiling...", entry.path().filename().string());
                        
                        RecompileShader(entry.path());
                        anyCompiled = true;
                    }
                }
            }
        }

        // If any shader successfully compiled, recreate pipelines
        if (anyCompiled) {
            Renderer2D::RecreatePipelines();
        }
    }

    void ShaderHotReloader::RecompileShader(const std::filesystem::path& path) {
        std::string filename = path.filename().string();
        std::string outSpvName = filename + ".spv";

        // Try compiling directly to the same directory
        std::string destSpv = (path.parent_path() / outSpvName).string();
        std::string command = "glslc \"" + path.string() + "\" -o \"" + destSpv + "\"";

        PX_CORE_INFO("ShaderHotReloader: Compiling via command: {0}", command);

        int result = std::system(command.c_str());
        if (result != 0) {
            PX_CORE_ERROR("ShaderHotReloader: Failed to compile shader: {0}. Ensure glslc is in your PATH.", filename);
            return;
        }

        PX_CORE_INFO("ShaderHotReloader: Successfully compiled shader to: {0}", destSpv);

        // Also copy it to other common build/runtime binary shader folders if they exist
        std::vector<std::string> outputDirs = {
            "shaders",
            "../shaders",
            "../../shaders",
            "build/bin/shaders",
            "../bin/shaders",
            "bin/shaders"
        };

        for (const auto& outDir : outputDirs) {
            if (std::filesystem::exists(outDir) && std::filesystem::is_directory(outDir)) {
                std::filesystem::path outPath = std::filesystem::path(outDir) / outSpvName;
                // Avoid self-copy
                if (std::filesystem::absolute(outPath) != std::filesystem::absolute(destSpv)) {
                    std::error_code ec;
                    std::filesystem::copy_file(destSpv, outPath, std::filesystem::copy_options::overwrite_existing, ec);
                    if (!ec) {
                        PX_CORE_INFO("ShaderHotReloader: Copied compiled binary to: {0}", outPath.string());
                    }
                }
            }
        }
    }

}
