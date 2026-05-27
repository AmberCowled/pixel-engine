#pragma once

#include <engine/core/EditorHistory.hpp>
#include <engine/ecs/Scene.hpp>
#include <engine/ecs/SceneSerializer.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>

namespace PixelEngine {

    class SceneSnapshotCommand : public EditorCommand {
    public:
        SceneSnapshotCommand(std::shared_ptr<Scene>& scene, const nlohmann::json& oldState, const nlohmann::json& newState, const std::string& name)
            : m_Scene(scene), m_OldState(oldState), m_NewState(newState), m_Name(name) {}

        void Execute() override {
            SceneSerializer serializer(*m_Scene);
            serializer.DeserializeFromJson(m_NewState);
        }

        void Undo() override {
            SceneSerializer serializer(*m_Scene);
            serializer.DeserializeFromJson(m_OldState);
        }

        std::string GetName() const override { return m_Name; }

    private:
        std::shared_ptr<Scene>& m_Scene;
        nlohmann::json m_OldState;
        nlohmann::json m_NewState;
        std::string m_Name;
    };

}
