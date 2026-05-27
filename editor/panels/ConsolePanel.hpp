#pragma once
#include "editor/EditorPanel.hpp"

namespace PixelEngine {

    class ConsolePanel : public EditorPanel {
    public:
        using EditorPanel::EditorPanel;

        void OnImGuiRender() override;
    };

}
