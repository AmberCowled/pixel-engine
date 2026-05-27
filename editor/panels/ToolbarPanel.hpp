#pragma once
#include "editor/EditorPanel.hpp"

namespace PixelEngine {

    class ToolbarPanel : public EditorPanel {
    public:
        using EditorPanel::EditorPanel;

        void OnImGuiRender() override;
    };

}
