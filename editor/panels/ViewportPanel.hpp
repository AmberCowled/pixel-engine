#pragma once
#include "editor/EditorPanel.hpp"

namespace PixelEngine {

    class ViewportPanel : public EditorPanel {
    public:
        using EditorPanel::EditorPanel;

        void OnImGuiRender() override;
    };

}
