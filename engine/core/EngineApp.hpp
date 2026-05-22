#pragma once

#include <string>
#include <memory>
#include <SDL3/SDL.h>

namespace PixelEngine {

    class VulkanContext;

    struct AppConfig {
        std::string Name = "Pixel Engine App";
        uint32_t Width = 1280;
        uint32_t Height = 720;
    };

    class EngineApp {
    public:
        EngineApp(const AppConfig& config = AppConfig());
        virtual ~EngineApp();

        void Run();
        void Close();

        virtual void OnUpdate(float deltaTime) {}
        virtual void OnRender() {}
        virtual void OnEvent(SDL_Event& event) {}

    protected:
        bool m_Running = true;
        SDL_Window* m_Window = nullptr;
        AppConfig m_Config;
        std::unique_ptr<VulkanContext> m_VulkanContext;

    private:
        void ProcessEvents();
    };

    // To be defined in client
    EngineApp* CreateApplication();

}
