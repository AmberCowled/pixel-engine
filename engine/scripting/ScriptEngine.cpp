#include "ScriptEngine.hpp"
#include <engine/base/Log.hpp>
#include <engine/ecs/Components.hpp>
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace PixelEngine {

    // Function pointers for hostfxr
    typedef int32_t (__stdcall *hostfxr_initialize_for_runtime_config_fn)(
        const wchar_t* runtime_config_path,
        const void* initialize_parameters,
        void** host_context_handle
    );

    typedef int32_t (__stdcall *hostfxr_get_runtime_delegate_fn)(
        void* host_context_handle,
        int runtime_delegate_type,
        void** delegate
    );

    typedef int32_t (__stdcall *hostfxr_close_fn)(
        void* host_context_handle
    );

    typedef int32_t (__stdcall *load_assembly_and_get_function_pointer_fn)(
        const wchar_t* assembly_path,
        const wchar_t* type_name,
        const wchar_t* method_name,
        const wchar_t* delegate_type_name,
        void* reserved,
        void** delegate
    );

    static HMODULE s_HostfxrLib = nullptr;
    static hostfxr_initialize_for_runtime_config_fn s_InitConfig = nullptr;
    static hostfxr_get_runtime_delegate_fn s_GetDelegate = nullptr;
    static hostfxr_close_fn s_Close = nullptr;
    static load_assembly_and_get_function_pointer_fn s_LoadAssemblyAndGetFunc = nullptr;
    static void* s_HostContextHandle = nullptr;

    // C# Method function pointers
    typedef void (__stdcall *ScriptEngine_Initialize_fn)(
        void* logCallback,
        void* getTransformCallback,
        void* setTransformCallback,
        void* hasComponentCallback,
        void* addComponentCallback,
        void* getVelocityCallback,
        void* setVelocityCallback
    );
    typedef int32_t (__stdcall *ScriptEngine_LoadAssembly_fn)(const char* assemblyPath);
    typedef int32_t (__stdcall *ScriptEngine_CreateInstance_fn)(uint64_t entityID, const char* className);
    typedef void (__stdcall *ScriptEngine_DestroyInstance_fn)(uint64_t entityID);
    typedef void (__stdcall *ScriptEngine_OnCreate_fn)(uint64_t entityID);
    typedef void (__stdcall *ScriptEngine_OnUpdate_fn)(uint64_t entityID, float dt);
    typedef void (__stdcall *ScriptEngine_OnCollisionEnter_fn)(uint64_t entityID, uint64_t otherID);
    typedef void (__stdcall *ScriptEngine_Reset_fn)();

    static ScriptEngine_Initialize_fn s_CS_Initialize = nullptr;
    static ScriptEngine_LoadAssembly_fn s_CS_LoadAssembly = nullptr;
    static ScriptEngine_CreateInstance_fn s_CS_CreateInstance = nullptr;
    static ScriptEngine_DestroyInstance_fn s_CS_DestroyInstance = nullptr;
    static ScriptEngine_OnCreate_fn s_CS_OnCreate = nullptr;
    static ScriptEngine_OnUpdate_fn s_CS_OnUpdate = nullptr;
    static ScriptEngine_OnCollisionEnter_fn s_CS_OnCollisionEnter = nullptr;
    static ScriptEngine_Reset_fn s_CS_Reset = nullptr;

    // Internal Call Callbacks
    struct Vector3 {
        float x, y, z;
    };

    static void __stdcall LogCallback(int level, const char* message) {
        if (level == 0) {
            PX_CORE_INFO("[C#] {0}", message);
        } else if (level == 1) {
            PX_CORE_WARN("[C#] {0}", message);
        } else if (level == 2) {
            PX_CORE_ERROR("[C#] {0}", message);
        }
    }

    static bool __stdcall GetTransformCallback(uint64_t entityID, Vector3* outTranslation, Vector3* outRotation, Vector3* outScale) {
        auto scene = ScriptEngine::GetActiveScene();
        if (!scene) return false;
        Entity entity = scene->GetEntityByUUID(UUID(entityID));
        if (!entity || !entity.HasComponent<TransformComponent>()) return false;

        auto& tc = entity.GetComponent<TransformComponent>();
        outTranslation->x = tc.Translation.x;
        outTranslation->y = tc.Translation.y;
        outTranslation->z = tc.Translation.z;

        outRotation->x = tc.Rotation.x;
        outRotation->y = tc.Rotation.y;
        outRotation->z = tc.Rotation.z;

        outScale->x = tc.Scale.x;
        outScale->y = tc.Scale.y;
        outScale->z = tc.Scale.z;
        return true;
    }

    static void __stdcall SetTransformCallback(uint64_t entityID, Vector3* translation, Vector3* rotation, Vector3* scale) {
        auto scene = ScriptEngine::GetActiveScene();
        if (!scene) return;
        Entity entity = scene->GetEntityByUUID(UUID(entityID));
        if (!entity || !entity.HasComponent<TransformComponent>()) return;

        auto& tc = entity.GetComponent<TransformComponent>();
        tc.Translation = glm::vec3(translation->x, translation->y, translation->z);
        tc.Rotation = glm::vec3(rotation->x, rotation->y, rotation->z);
        tc.Scale = glm::vec3(scale->x, scale->y, scale->z);
    }

    static bool __stdcall HasComponentCallback(uint64_t entityID, int componentType) {
        auto scene = ScriptEngine::GetActiveScene();
        if (!scene) return false;
        Entity entity = scene->GetEntityByUUID(UUID(entityID));
        if (!entity) return false;

        if (componentType == 0) return entity.HasComponent<TransformComponent>();
        if (componentType == 1) return entity.HasComponent<SpriteRendererComponent>();
        if (componentType == 2) return entity.HasComponent<MeshRendererComponent>();
        if (componentType == 3) return entity.HasComponent<VelocityComponent>();
        if (componentType == 4) return entity.HasComponent<HierarchyComponent>();
        if (componentType == 5) return entity.HasComponent<SpriteAnimationComponent>();
        if (componentType == 6) return entity.HasComponent<TilemapComponent>();
        if (componentType == 7) return entity.HasComponent<AnimatorComponent>();
        if (componentType == 8) return entity.HasComponent<AudioSourceComponent>();
        return false;
    }

    static void __stdcall AddComponentCallback(uint64_t entityID, int componentType) {
        auto scene = ScriptEngine::GetActiveScene();
        if (!scene) return;
        Entity entity = scene->GetEntityByUUID(UUID(entityID));
        if (!entity) return;

        if (componentType == 0 && !entity.HasComponent<TransformComponent>()) entity.AddComponent<TransformComponent>();
        if (componentType == 1 && !entity.HasComponent<SpriteRendererComponent>()) entity.AddComponent<SpriteRendererComponent>();
        if (componentType == 2 && !entity.HasComponent<MeshRendererComponent>()) entity.AddComponent<MeshRendererComponent>();
        if (componentType == 3 && !entity.HasComponent<VelocityComponent>()) entity.AddComponent<VelocityComponent>();
        if (componentType == 4 && !entity.HasComponent<HierarchyComponent>()) entity.AddComponent<HierarchyComponent>();
        if (componentType == 5 && !entity.HasComponent<SpriteAnimationComponent>()) entity.AddComponent<SpriteAnimationComponent>();
        if (componentType == 6 && !entity.HasComponent<TilemapComponent>()) entity.AddComponent<TilemapComponent>();
        if (componentType == 7 && !entity.HasComponent<AnimatorComponent>()) entity.AddComponent<AnimatorComponent>();
        if (componentType == 8 && !entity.HasComponent<AudioSourceComponent>()) entity.AddComponent<AudioSourceComponent>();
    }

    static bool __stdcall GetVelocityCallback(uint64_t entityID, Vector3* outLinear, Vector3* outAngular) {
        auto scene = ScriptEngine::GetActiveScene();
        if (!scene) return false;
        Entity entity = scene->GetEntityByUUID(UUID(entityID));
        if (!entity || !entity.HasComponent<VelocityComponent>()) return false;

        auto& vc = entity.GetComponent<VelocityComponent>();
        outLinear->x = vc.Linear.x;
        outLinear->y = vc.Linear.y;
        outLinear->z = vc.Linear.z;

        outAngular->x = vc.Angular.x;
        outAngular->y = vc.Angular.y;
        outAngular->z = vc.Angular.z;
        return true;
    }

    static void __stdcall SetVelocityCallback(uint64_t entityID, Vector3* linear, Vector3* angular) {
        auto scene = ScriptEngine::GetActiveScene();
        if (!scene) return;
        Entity entity = scene->GetEntityByUUID(UUID(entityID));
        if (!entity || !entity.HasComponent<VelocityComponent>()) return;

        auto& vc = entity.GetComponent<VelocityComponent>();
        vc.Linear = glm::vec3(linear->x, linear->y, linear->z);
        vc.Angular = glm::vec3(angular->x, angular->y, angular->z);
    }

    bool ScriptEngine::LoadHostfxr() {
        if (s_HostfxrLib) return true;

        std::filesystem::path hostfxrPath = "C:\\Program Files\\dotnet\\host\\fxr";
        if (!std::filesystem::exists(hostfxrPath)) {
            PX_CORE_ERROR("Dotnet installation directory not found at C:\\Program Files\\dotnet\\host\\fxr");
            return false;
        }

        std::filesystem::path latestVersionDir;
        std::string latestVersion = "";
        for (const auto& entry : std::filesystem::directory_iterator(hostfxrPath)) {
            if (entry.is_directory()) {
                std::string ver = entry.path().filename().string();
                if (ver > latestVersion) {
                    latestVersion = ver;
                    latestVersionDir = entry.path();
                }
            }
        }

        if (latestVersionDir.empty()) {
            PX_CORE_ERROR("No dotnet hostfxr versions found.");
            return false;
        }

        std::filesystem::path dllPath = latestVersionDir / "hostfxr.dll";
        if (!std::filesystem::exists(dllPath)) {
            PX_CORE_ERROR("hostfxr.dll not found at: {0}", dllPath.string());
            return false;
        }

        s_HostfxrLib = LoadLibraryW(dllPath.wstring().c_str());
        if (!s_HostfxrLib) {
            PX_CORE_ERROR("Failed to load hostfxr.dll");
            return false;
        }

        s_InitConfig = (hostfxr_initialize_for_runtime_config_fn)GetProcAddress(s_HostfxrLib, "hostfxr_initialize_for_runtime_config");
        s_GetDelegate = (hostfxr_get_runtime_delegate_fn)GetProcAddress(s_HostfxrLib, "hostfxr_get_runtime_delegate");
        s_Close = (hostfxr_close_fn)GetProcAddress(s_HostfxrLib, "hostfxr_close");

        return s_InitConfig && s_GetDelegate && s_Close;
    }

    void ScriptEngine::Init(const std::string& coreAssemblyPath) {
        if (s_Initialized) return;

        s_CoreAssemblyPath = coreAssemblyPath;

        if (!LoadHostfxr()) {
            PX_CORE_ERROR("ScriptEngine: hostfxr loading failed.");
            return;
        }

        std::filesystem::path coreAssembly(coreAssemblyPath);
        std::filesystem::path configPath = coreAssembly.parent_path() / "PixelEngineScripting.runtimeconfig.json";
        if (!std::filesystem::exists(configPath)) {
            std::ofstream fout(configPath);
            fout << R"({
  "runtimeOptions": {
    "tfm": "net8.0",
    "frameworks": [
      {
        "name": "Microsoft.NETCore.App",
        "version": "8.0.0"
      }
    ]
  }
}
)";
            fout.close();
        }

        void* hostContextHandle = nullptr;
        int rc = s_InitConfig(configPath.wstring().c_str(), nullptr, &hostContextHandle);
        if (rc != 0 || !hostContextHandle) {
            PX_CORE_ERROR("hostfxr_initialize_for_runtime_config failed: {0:x}", rc);
            return;
        }
        s_HostContextHandle = hostContextHandle;

        rc = s_GetDelegate(s_HostContextHandle, 5, (void**)&s_LoadAssemblyAndGetFunc);
        if (rc != 0 || !s_LoadAssemblyAndGetFunc) {
            PX_CORE_ERROR("Failed to get load_assembly_and_get_function_pointer delegate: {0:x}", rc);
            s_Close(s_HostContextHandle);
            s_HostContextHandle = nullptr;
            return;
        }

        std::wstring corePathW = coreAssembly.wstring();
        
        rc = s_LoadAssemblyAndGetFunc(
            corePathW.c_str(),
            L"PixelEngine.ScriptEngine, PixelEngineScripting",
            L"Initialize",
            (const wchar_t*)-1,
            nullptr,
            (void**)&s_CS_Initialize
        );
        if (rc != 0 || !s_CS_Initialize) {
            PX_CORE_ERROR("Failed to get function pointer to PixelEngine.ScriptEngine.Initialize: {0:x}", rc);
            return;
        }

        s_LoadAssemblyAndGetFunc(corePathW.c_str(), L"PixelEngine.ScriptEngine, PixelEngineScripting", L"LoadAssembly", (const wchar_t*)-1, nullptr, (void**)&s_CS_LoadAssembly);
        s_LoadAssemblyAndGetFunc(corePathW.c_str(), L"PixelEngine.ScriptEngine, PixelEngineScripting", L"CreateInstance", (const wchar_t*)-1, nullptr, (void**)&s_CS_CreateInstance);
        s_LoadAssemblyAndGetFunc(corePathW.c_str(), L"PixelEngine.ScriptEngine, PixelEngineScripting", L"DestroyInstance", (const wchar_t*)-1, nullptr, (void**)&s_CS_DestroyInstance);
        s_LoadAssemblyAndGetFunc(corePathW.c_str(), L"PixelEngine.ScriptEngine, PixelEngineScripting", L"OnCreate", (const wchar_t*)-1, nullptr, (void**)&s_CS_OnCreate);
        s_LoadAssemblyAndGetFunc(corePathW.c_str(), L"PixelEngine.ScriptEngine, PixelEngineScripting", L"OnUpdate", (const wchar_t*)-1, nullptr, (void**)&s_CS_OnUpdate);
        s_LoadAssemblyAndGetFunc(corePathW.c_str(), L"PixelEngine.ScriptEngine, PixelEngineScripting", L"OnCollisionEnter", (const wchar_t*)-1, nullptr, (void**)&s_CS_OnCollisionEnter);
        s_LoadAssemblyAndGetFunc(corePathW.c_str(), L"PixelEngine.ScriptEngine, PixelEngineScripting", L"Reset", (const wchar_t*)-1, nullptr, (void**)&s_CS_Reset);

        s_CS_Initialize(
            (void*)LogCallback,
            (void*)GetTransformCallback,
            (void*)SetTransformCallback,
            (void*)HasComponentCallback,
            (void*)AddComponentCallback,
            (void*)GetVelocityCallback,
            (void*)SetVelocityCallback
        );

        s_Initialized = true;
    }

    void ScriptEngine::Shutdown() {
        if (!s_Initialized) return;

        if (s_HostContextHandle && s_Close) {
            s_Close(s_HostContextHandle);
            s_HostContextHandle = nullptr;
        }
        if (s_HostfxrLib) {
            FreeLibrary(s_HostfxrLib);
            s_HostfxrLib = nullptr;
        }

        s_Initialized = false;
    }

    bool ScriptEngine::LoadGameAssembly(const std::string& assemblyPath) {
        if (!s_Initialized || !s_CS_LoadAssembly) return false;
        s_GameAssemblyPath = assemblyPath;
        return s_CS_LoadAssembly(assemblyPath.c_str()) == 1;
    }

    void ScriptEngine::OnUpdate(float dt) {
        if (!s_Initialized || !s_CS_OnUpdate || !s_ActiveScene) return;

        auto view = s_ActiveScene->Reg().view<ScriptComponent>();
        for (auto entityID : view) {
            Entity entity = { entityID, s_ActiveScene.get() };
            uint64_t uuid = static_cast<uint64_t>(entity.GetComponent<IDComponent>().ID);
            s_CS_OnUpdate(uuid, dt);
        }
    }

    void ScriptEngine::OnCreateEntity(Entity entity) {
        if (!s_Initialized || !s_CS_CreateInstance || !s_CS_OnCreate) return;
        if (entity.HasComponent<ScriptComponent>()) {
            auto& sc = entity.GetComponent<ScriptComponent>();
            uint64_t entityID = static_cast<uint64_t>(entity.GetComponent<IDComponent>().ID);
            if (s_CS_CreateInstance(entityID, sc.ClassName.c_str()) == 1) {
                s_CS_OnCreate(entityID);
            }
        }
    }

    void ScriptEngine::OnDestroyEntity(Entity entity) {
        if (!s_Initialized || !s_CS_DestroyInstance) return;
        if (entity.HasComponent<ScriptComponent>()) {
            uint64_t entityID = static_cast<uint64_t>(entity.GetComponent<IDComponent>().ID);
            s_CS_DestroyInstance(entityID);
        }
    }

    void ScriptEngine::OnCollisionEnter(Entity entity, Entity other) {
        if (!s_Initialized || !s_CS_OnCollisionEnter) return;
        if (entity.HasComponent<ScriptComponent>()) {
            uint64_t entityID = static_cast<uint64_t>(entity.GetComponent<IDComponent>().ID);
            uint64_t otherID = static_cast<uint64_t>(other.GetComponent<IDComponent>().ID);
            s_CS_OnCollisionEnter(entityID, otherID);
        }
    }

    void ScriptEngine::Reset() {
        if (s_CS_Reset) {
            s_CS_Reset();
        }
    }

}
