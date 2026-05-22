#include <engine/core/EngineApp.hpp>
#include <engine/core/Log.hpp>

class SandboxApp : public PixelEngine::EngineApp {
public:
    SandboxApp() : PixelEngine::EngineApp({"Pixel Sandbox", 1280, 720}) {
        PX_INFO("Sandbox App Started.");
    }

    ~SandboxApp() {
        PX_INFO("Sandbox App Shutdown.");
    }

    void OnUpdate(float deltaTime) override {
        // Update logic here
    }

    void OnRender() override {
        // Render logic here
    }
};

int main(int argc, char* argv[]) {
    SandboxApp* app = new SandboxApp();
    app->Run();
    delete app;
    return 0;
}
