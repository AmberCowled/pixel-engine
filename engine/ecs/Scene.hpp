#pragma once

#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <engine/core/UUID.hpp>
#include <glm/glm.hpp>
#include <memory>

namespace PixelEngine {

    class Entity;

    class Scene {
    public:
        Scene();
        ~Scene();

        static std::shared_ptr<Scene> Clone(std::shared_ptr<Scene> source);

        Entity CreateEntity(const std::string& name = std::string());
        Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
        void DestroyEntity(Entity entity);

        void OnUpdate(float deltaTime);

        glm::mat4 GetWorldTransform(Entity entity);
        Entity GetEntityByUUID(UUID uuid);

        entt::registry& Reg() { return m_Registry; }
        const std::unordered_map<UUID, entt::entity>& GetEntityMap() const { return m_EntityMap; }

    private:
        entt::registry m_Registry;
        std::unordered_map<UUID, entt::entity> m_EntityMap;

        friend class Entity;
        friend class RenderSystem;
        friend class SceneSerializer;
    };

}
