#pragma once

#include <entt/entt.hpp>
#include <string>

namespace PixelEngine {

    class Entity;

    class Scene {
    public:
        Scene();
        ~Scene();

        Entity CreateEntity(const std::string& name = std::string());
        void DestroyEntity(Entity entity);

        void OnUpdate(float deltaTime);

        entt::registry& Reg() { return m_Registry; }

    private:
        entt::registry m_Registry;

        friend class Entity;
        friend class RenderSystem;
    };

}
