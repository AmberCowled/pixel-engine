#pragma once

#include "Scene.hpp"
#include <string>
#include <nlohmann/json.hpp>

namespace PixelEngine {

    class SceneSerializer {
    public:
        SceneSerializer(Scene& scene);

        void Serialize(const std::string& filepath);
        bool Deserialize(const std::string& filepath);

        nlohmann::json SerializeToJson();
        bool DeserializeFromJson(const nlohmann::json& json);

    private:
        Scene& m_Scene;
    };

}
