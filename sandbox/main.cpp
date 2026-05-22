#include <engine/core/EngineApp.hpp>
#include <engine/base/Log.hpp>
#include <imgui.h>

class SandboxApp : public PixelEngine::EngineApp {
public:
    SandboxApp() : PixelEngine::EngineApp({"Pixel Sandbox", 1280, 720}) {
        PX_INFO("Sandbox App Started.");
    }

    ~SandboxApp() {
        PX_INFO("Sandbox App Shutdown.");
    }

    void OnUpdate(float deltaTime) override {
        ImGui::Begin("Engine Status");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("DeltaTime: %.3f ms", deltaTime * 1000.0f);
        ImGui::End();

        ImGui::ShowDemoWindow();
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
