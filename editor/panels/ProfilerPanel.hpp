#pragma once
#include "editor/EditorPanel.hpp"

namespace PixelEngine {

    class ProfilerPanel : public EditorPanel {
    public:
        using EditorPanel::EditorPanel;

        void OnImGuiRender() override;
    };

}
