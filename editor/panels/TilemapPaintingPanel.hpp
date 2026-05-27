#pragma once
#include "editor/EditorPanel.hpp"

namespace PixelEngine {

    class TilemapPaintingPanel : public EditorPanel {
    public:
        using EditorPanel::EditorPanel;

        void OnImGuiRender() override;
    };

}
