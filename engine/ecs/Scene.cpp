#include "Scene.hpp"
#include "Entity.hpp"
#include "Components.hpp"

namespace PixelEngine {

    Scene::Scene() {}

    Scene::~Scene() {}

    Entity Scene::CreateEntity(const std::string& name) {
        Entity entity = {m_Registry.create(), this};
        entity.AddComponent<IDComponent>();
        entity.AddComponent<TransformComponent>();
        auto& tag = entity.AddComponent<TagComponent>();
        tag.Tag = name.empty() ? "Entity" : name;
        return entity;
    }

    void Scene::DestroyEntity(Entity entity) {
        m_Registry.destroy(entity);
    }

    void Scene::OnUpdate(float deltaTime) {
        // Future: Update systems (Physics, Scripting, etc.)
    }

    // Entity implementation
    Entity::Entity(entt::entity handle, Scene* scene) : m_EntityHandle(handle), m_Scene(scene) {}

}
