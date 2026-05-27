#pragma once
#include "editor/EditorPanel.hpp"

namespace PixelEngine {

    class AudioMixerPanel : public EditorPanel {
    public:
        using EditorPanel::EditorPanel;

        void OnImGuiRender() override;
    };

}
