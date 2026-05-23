#pragma once

#include "Scene.hpp"
#include <string>

namespace PixelEngine {

    class SceneSerializer {
    public:
        SceneSerializer(Scene& scene);

        void Serialize(const std::string& filepath);
        bool Deserialize(const std::string& filepath);

    private:
        Scene& m_Scene;
    };

}
