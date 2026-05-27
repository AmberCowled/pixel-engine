#pragma once
#include "editor/EditorPanel.hpp"

namespace PixelEngine {

    class AnimationEditorPanel : public EditorPanel {
    public:
        using EditorPanel::EditorPanel;

        void OnImGuiRender() override;
    };

}
