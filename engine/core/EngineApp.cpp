#include "EngineApp.hpp"
#include "Log.hpp"

namespace PixelEngine {

    EngineApp::EngineApp(const AppConfig& config) 
        : m_Config(config) {
        
        Log::Init();
        PX_CORE_INFO("Initializing Engine...");

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == false) {
            PX_CORE_CRITICAL("SDL could not initialize! SDL_Error: {0}", SDL_GetError());
            m_Running = false;
            return;
        }

        m_Window = SDL_CreateWindow(
            m_Config.Name.c_str(),
            m_Config.Width,
            m_Config.Height,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
        );

        if (!m_Window) {
            PX_CORE_CRITICAL("Window could not be created! SDL_Error: {0}", SDL_GetError());
            m_Running = false;
            return;
        }

        PX_CORE_INFO("Engine Initialized Successfully.");
    }

    EngineApp::~EngineApp() {
        if (m_Window) {
            SDL_DestroyWindow(m_Window);
        }
        SDL_Quit();
        PX_CORE_INFO("Engine Shutdown.");
    }

    void EngineApp::Run() {
        uint64_t lastTime = SDL_GetTicks();

        while (m_Running) {
            uint64_t currentTime = SDL_GetTicks();
            float deltaTime = (currentTime - lastTime) / 1000.0f;
            lastTime = currentTime;

            ProcessEvents();

            if (m_Running) {
                OnUpdate(deltaTime);
                OnRender();
            }
        }
    }

    void EngineApp::Close() {
        m_Running = false;
    }

    void EngineApp::ProcessEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_Running = false;
            }

            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(m_Window)) {
                m_Running = false;
            }

            OnEvent(event);
        }
    }

}
