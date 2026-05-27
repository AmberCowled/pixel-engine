#pragma once
#include "editor/EditorPanel.hpp"

namespace PixelEngine {

    class HierarchyPanel : public EditorPanel {
    public:
        using EditorPanel::EditorPanel;

        void OnImGuiRender() override;

    private:
        void DrawEntityNode(Entity entity);
        void DrawEntityContextMenu(Entity entity);
        bool EntityMatchesFilters(Entity entity, const std::string& searchQuery);
        bool IsDescendantOf(Entity entity, UUID potentialParentUUID);
    };

}
