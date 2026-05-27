#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <atomic>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <functional>
#include <engine/renderer/Camera.hpp>
#include <engine/renderer/OffscreenTarget.hpp>
#include <engine/ecs/Scene.hpp>
#include <engine/ecs/Entity.hpp>
#include <engine/core/UUID.hpp>

struct SDL_Window;

namespace PixelEngine {

    class VulkanContext;

    enum class SceneState { Edit = 0, Play = 1, Pause = 2 };
    enum class HierarchyFilter { All = 0, Script, Sprite, Audio, Tilemap };
    enum class AssetFilter { All = 0, Textures, Audio, Scripts, Prefabs, Scenes };
    enum class BrushType { Paint = 0, Erase = 1 };

    struct EditorContext {
        VulkanContext* VulkanCtx = nullptr;
        SDL_Window* Window = nullptr;

        // Active states
        std::shared_ptr<Scene> ActiveScene;
        std::shared_ptr<Scene> EditorScene;
        Entity SelectedEntity;
        SceneState CurrentSceneState = SceneState::Edit;

        // Project settings
        bool ProjectLoaded = false;
        std::string ProjectPath = "";
        std::string ProjectName = "";
        std::string ActiveScenePath = "";
        std::vector<std::string> RecentProjects;

        // Selection & navigation paths
        std::filesystem::path CurrentDirectory;
        std::filesystem::path SelectedAssetPath;

        // UI trigger states
        bool TriggerRenamePopup = false;
        bool TriggerAssetRenamePopup = false;
        bool TriggerCreateFolderPopup = false;
        bool TriggerCreateScriptPopup = false;
        bool TriggerCreateMetadataPopup = false;
        bool ShowExitPopup = false;
        bool ShowCloseProjectPopup = false;
        bool ResetLayout = false;

        // Focused panels
        bool HierarchyFocused = false;
        bool AssetBrowserFocused = false;

        // Search & filtering buffers
        char HierarchySearchBuffer[256]{};
        HierarchyFilter CurrentHierarchyFilter = HierarchyFilter::All;

        char AssetSearchBuffer[256]{};
        AssetFilter CurrentAssetFilter = AssetFilter::All;

        // Gizmo & Viewport settings
        int GizmoType = -1; // -1 means none
        glm::vec2 ViewportSize{ 0.0f, 0.0f };
        VkDescriptorSet ViewportDescriptorSet = VK_NULL_HANDLE;
        std::unique_ptr<OffscreenTarget> OffscreenBuffer;
        Camera EditorCamera;
        glm::vec2 CameraTarget{ 0.0f, 0.0f };
        float CameraDistance = 10.0f;
        bool ViewportFocused = false;
        bool ViewportHovered = false;

        // Tilemap Painting settings
        BrushType CurrentBrushType = BrushType::Paint;
        uint32_t SelectedTileIndex = 1;
        nlohmann::json BeforePaintState;
        bool IsPainting = false;

        // Background threads/reloading
        std::atomic<bool> AssemblyReloadPending{ false };

        // Test/Global variables
        float DeltaTime = 0.0f;
        bool PixelSnapping = true;
        float Rotation = 0.0f;
        float RotationSpeed = 50.0f;
        UUID TestTexture = 0;

        // Callback functions
        std::function<void(const std::string&)> LoadProjectCallback;
        std::function<void(const std::string&, const std::string&)> CreateNewProjectCallback;
        std::function<void(const std::string&)> InstantiatePrefabCallback;
        std::function<void(UUID, const std::filesystem::path&)> SaveEntityAsPrefabCallback;
        std::function<void()> CompileCSProjectAsyncCallback;

        std::function<void(Entity, UUID)> ApplyPrefabOverridesCallback;
        std::function<void(Entity, UUID)> RevertPrefabOverridesCallback;

        std::function<void(Entity)> GroupEntityCallback;
        std::function<void(Entity, bool)> ReorderEntityCallback;
        std::function<Entity(const std::string&, UUID)> CreatePresetEntityCallback;
        std::function<Entity(Entity, UUID, const std::string&)> DuplicateSubtreeCallback;
        std::function<void(Entity)> DeleteEntityCallback;
    };

}
