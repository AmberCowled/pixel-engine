#pragma once
#include "editor/EditorPanel.hpp"
#include <filesystem>

namespace PixelEngine {

    class AssetBrowserPanel : public EditorPanel {
    public:
        using EditorPanel::EditorPanel;

        void OnImGuiRender() override;

    private:
        void DrawDirectoryGridItem(const std::filesystem::path& path, const std::filesystem::path& assetRoot);
        void DrawAssetGridItem(const std::filesystem::path& path, const std::filesystem::path& assetRoot);
        bool MatchAssetFilter(const std::filesystem::path& path);
        bool MatchAssetSearch(const std::filesystem::path& path, const std::string& query);
    };

}
