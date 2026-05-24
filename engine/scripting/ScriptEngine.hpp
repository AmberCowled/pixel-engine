#pragma once

#include <string>
#include <memory>
#include <engine/ecs/Entity.hpp>
#include <engine/ecs/Scene.hpp>

namespace PixelEngine {

    class ScriptEngine {
    public:
        static void Init(const std::string& coreAssemblyPath);
        static void Shutdown();

        static bool LoadGameAssembly(const std::string& assemblyPath);
        
        static void OnUpdate(float dt);
        static void OnCreateEntity(Entity entity);
        static void OnDestroyEntity(Entity entity);
        static void OnCollisionEnter(Entity entity, Entity other);
        static void Reset();

        static void SetActiveScene(std::shared_ptr<Scene> scene) { s_ActiveScene = scene; }
        static std::shared_ptr<Scene> GetActiveScene() { return s_ActiveScene; }

        static bool IsInitialized() { return s_Initialized; }

    private:
        static bool LoadHostfxr();

    private:
        inline static bool s_Initialized = false;
        inline static std::string s_CoreAssemblyPath = "";
        inline static std::string s_GameAssemblyPath = "";
        inline static std::shared_ptr<Scene> s_ActiveScene = nullptr;
    };

}
