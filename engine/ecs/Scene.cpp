#include "Scene.hpp"
#include "Entity.hpp"
#include "Components.hpp"

namespace PixelEngine {

    Scene::Scene() {}

    Scene::~Scene() {}

    Entity Scene::CreateEntity(const std::string& name) {
        return CreateEntityWithUUID(UUID(), name);
    }

    Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name) {
        Entity entity = {m_Registry.create(), this};
        entity.AddComponent<IDComponent>();
        entity.GetComponent<IDComponent>().ID = uuid;
        entity.AddComponent<TransformComponent>();
        auto& tag = entity.AddComponent<TagComponent>();
        tag.Tag = name.empty() ? "Entity" : name;

        m_EntityMap[uuid] = entity;
        return entity;
    }

    void Scene::DestroyEntity(Entity entity) {
        if (entity.HasComponent<IDComponent>()) {
            m_EntityMap.erase(entity.GetComponent<IDComponent>().ID);
        }
        m_Registry.destroy(entity);
    }

    void Scene::OnUpdate(float deltaTime) {
        // 1. Physics / Movement System
        auto physicsView = m_Registry.view<TransformComponent, VelocityComponent>();
        for (auto entity : physicsView) {
            auto& transform = physicsView.get<TransformComponent>(entity);
            auto& velocity = physicsView.get<VelocityComponent>(entity);

            transform.Translation += velocity.Linear * deltaTime;
            transform.Rotation += velocity.Angular * deltaTime;
        }

        // 2. Sprite Animation System
        auto animView = m_Registry.view<SpriteRendererComponent, SpriteAnimationComponent>();
        for (auto entity : animView) {
            auto& sprite = animView.get<SpriteRendererComponent>(entity);
            auto& anim = animView.get<SpriteAnimationComponent>(entity);

            if (!anim.Playing || anim.Textures.empty()) continue;

            anim.Timer += deltaTime;
            if (anim.Timer >= anim.FrameTime) {
                anim.Timer -= anim.FrameTime;
                anim.CurrentFrame = anim.CurrentFrame + 1;
                if (anim.CurrentFrame >= static_cast<int>(anim.Textures.size())) {
                    if (anim.Loop) {
                        anim.CurrentFrame = 0;
                    } else {
                        anim.CurrentFrame = static_cast<int>(anim.Textures.size()) - 1;
                        anim.Playing = false;
                    }
                }
                sprite.Mat.TextureID = anim.Textures[anim.CurrentFrame];
            }
        }
    }

    glm::mat4 Scene::GetWorldTransform(Entity entity) {
        if (!entity) return glm::mat4(1.0f);

        glm::mat4 transform = glm::mat4(1.0f);
        if (entity.HasComponent<TransformComponent>()) {
            transform = entity.GetComponent<TransformComponent>().GetTransform();
        }

        if (entity.HasComponent<HierarchyComponent>()) {
            auto& hc = entity.GetComponent<HierarchyComponent>();
            if (hc.Parent != 0) {
                auto parentEntity = GetEntityByUUID(hc.Parent);
                if (parentEntity) {
                    return GetWorldTransform(parentEntity) * transform;
                }
            }
        }

        return transform;
    }

    Entity Scene::GetEntityByUUID(UUID uuid) {
        auto it = m_EntityMap.find(uuid);
        if (it != m_EntityMap.end()) {
            return {it->second, this};
        }
        return {};
    }

    Entity::Entity(entt::entity handle, Scene* scene) : m_EntityHandle(handle), m_Scene(scene) {}

}
