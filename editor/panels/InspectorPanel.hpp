#pragma once
#include "editor/EditorPanel.hpp"

namespace PixelEngine {

    class InspectorPanel : public EditorPanel {
    public:
        using EditorPanel::EditorPanel;

        void OnImGuiRender() override;
    };

}
