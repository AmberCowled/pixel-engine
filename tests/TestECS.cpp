#include <gtest/gtest.h>
#include <engine/ecs/Scene.hpp>
#include <engine/ecs/Entity.hpp>
#include <engine/ecs/Components.hpp>

class TestECSSuite : public ::testing::Test {
protected:
    void SetUp() override {
        m_Scene = std::make_shared<PixelEngine::Scene>();
    }

    void TearDown() override {
        m_Scene.reset();
    }

    std::shared_ptr<PixelEngine::Scene> m_Scene;
};

TEST_F(TestECSSuite, EntityAllocation) {
    auto entity1 = m_Scene->CreateEntity("TestEntity1");
    auto entity2 = m_Scene->CreateEntity("TestEntity2");

    EXPECT_TRUE(entity1);
    EXPECT_TRUE(entity2);
    EXPECT_NE(entity1, entity2);

    auto& tag1 = entity1.GetComponent<PixelEngine::TagComponent>().Tag;
    auto& tag2 = entity2.GetComponent<PixelEngine::TagComponent>().Tag;

    EXPECT_EQ(tag1, "TestEntity1");
    EXPECT_EQ(tag2, "TestEntity2");
}

TEST_F(TestECSSuite, AddRemoveHasComponent) {
    auto entity = m_Scene->CreateEntity("ComponentTester");

    // Initially should not have velocity or transform components
    EXPECT_FALSE(entity.HasComponent<PixelEngine::VelocityComponent>());
    
    // Add velocity component
    entity.AddComponent<PixelEngine::VelocityComponent>(glm::vec3(1.0f, 2.0f, 3.0f));
    EXPECT_TRUE(entity.HasComponent<PixelEngine::VelocityComponent>());

    auto& velocity = entity.GetComponent<PixelEngine::VelocityComponent>();
    EXPECT_EQ(velocity.Linear.x, 1.0f);
    EXPECT_EQ(velocity.Linear.y, 2.0f);
    EXPECT_EQ(velocity.Linear.z, 3.0f);

    // Remove velocity component
    entity.RemoveComponent<PixelEngine::VelocityComponent>();
    EXPECT_FALSE(entity.HasComponent<PixelEngine::VelocityComponent>());
}

TEST_F(TestECSSuite, EntityParenting) {
    auto parent = m_Scene->CreateEntity("Parent");
    auto child = m_Scene->CreateEntity("Child");

    auto parentID = parent.GetComponent<PixelEngine::IDComponent>().ID;
    auto childID = child.GetComponent<PixelEngine::IDComponent>().ID;

    // Attach child to parent
    child.AddComponent<PixelEngine::HierarchyComponent>(parentID);
    parent.AddComponent<PixelEngine::HierarchyComponent>();
    parent.GetComponent<PixelEngine::HierarchyComponent>().Children.push_back(childID);

    EXPECT_TRUE(child.HasComponent<PixelEngine::HierarchyComponent>());
    EXPECT_TRUE(parent.HasComponent<PixelEngine::HierarchyComponent>());

    auto& childHierarchy = child.GetComponent<PixelEngine::HierarchyComponent>();
    EXPECT_EQ(childHierarchy.Parent, parentID);

    auto& parentHierarchy = parent.GetComponent<PixelEngine::HierarchyComponent>();
    ASSERT_EQ(parentHierarchy.Children.size(), 1u);
    EXPECT_EQ(parentHierarchy.Children[0], childID);
}

TEST_F(TestECSSuite, SceneCloning) {
    auto sourceEntity = m_Scene->CreateEntity("Original");
    sourceEntity.GetComponent<PixelEngine::TransformComponent>().Translation = glm::vec3(5.0f, 10.0f, 15.0f);

    auto clonedScene = PixelEngine::Scene::Clone(m_Scene);
    ASSERT_NE(clonedScene, nullptr);

    auto view = clonedScene->Reg().view<PixelEngine::TagComponent>();
    bool foundClone = false;
    for (auto ent : view) {
        PixelEngine::Entity entity = { ent, clonedScene.get() };
        if (entity.GetComponent<PixelEngine::TagComponent>().Tag == "Original") {
            foundClone = true;
            EXPECT_TRUE(entity.HasComponent<PixelEngine::TransformComponent>());
            auto& tc = entity.GetComponent<PixelEngine::TransformComponent>();
            EXPECT_EQ(tc.Translation.x, 5.0f);
            EXPECT_EQ(tc.Translation.y, 10.0f);
            EXPECT_EQ(tc.Translation.z, 15.0f);
        }
    }
    EXPECT_TRUE(foundClone);
}
