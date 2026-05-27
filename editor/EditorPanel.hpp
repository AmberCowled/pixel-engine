#pragma once
#include "EditorContext.hpp"

namespace PixelEngine {

    class EditorPanel {
    public:
        EditorPanel(EditorContext& context) : m_Context(context) {}
        virtual ~EditorPanel() = default;

        virtual void OnImGuiRender() = 0;

    protected:
        EditorContext& m_Context;
    };

}
