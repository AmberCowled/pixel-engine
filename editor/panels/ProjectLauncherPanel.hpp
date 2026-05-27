#pragma once
#include "editor/EditorPanel.hpp"

namespace PixelEngine {

    class ProjectLauncherPanel : public EditorPanel {
    public:
        using EditorPanel::EditorPanel;

        void OnImGuiRender() override;
    };

}
