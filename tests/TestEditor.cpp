#include <gtest/gtest.h>
#include <engine/ecs/Scene.hpp>
#include <engine/ecs/Entity.hpp>
#include <engine/ecs/Components.hpp>
#include <editor/EditorUtils.hpp>

class TestEditorSuite : public ::testing::Test {
protected:
    void SetUp() override {
        m_Scene = std::make_shared<PixelEngine::Scene>();
    }

    void TearDown() override {
        m_Scene.reset();
    }

    std::shared_ptr<PixelEngine::Scene> m_Scene;
};

TEST_F(TestEditorSuite, PrefabDetection) {
    auto parent = m_Scene->CreateEntity("Parent");
    auto child = m_Scene->CreateEntity("Child");
    
    auto parentID = parent.GetComponent<PixelEngine::IDComponent>().ID;
    auto childID = child.GetComponent<PixelEngine::IDComponent>().ID;

    // Set parenting
    child.AddComponent<PixelEngine::HierarchyComponent>(parentID);
    parent.AddComponent<PixelEngine::HierarchyComponent>();
    parent.GetComponent<PixelEngine::HierarchyComponent>().Children.push_back(childID);

    PixelEngine::UUID outPrefabID = 0;
    
    // Parent and child are not prefabs yet
    EXPECT_FALSE(PixelEngine::IsPartOfPrefab(parent, m_Scene.get(), outPrefabID));
    EXPECT_FALSE(PixelEngine::IsPartOfPrefab(child, m_Scene.get(), outPrefabID));

    // Assign PrefabComponent to parent
    PixelEngine::UUID prefabID = 12345;
    parent.AddComponent<PixelEngine::PrefabComponent>(prefabID, parentID);

    // Parent is a prefab root
    EXPECT_TRUE(PixelEngine::IsPartOfPrefab(parent, m_Scene.get(), outPrefabID));
    EXPECT_EQ(outPrefabID, prefabID);

    // Child is recursively identified as part of the parent's prefab
    outPrefabID = 0;
    EXPECT_TRUE(PixelEngine::IsPartOfPrefab(child, m_Scene.get(), outPrefabID));
    EXPECT_EQ(outPrefabID, prefabID);
}

TEST_F(TestEditorSuite, PrefabOverrides) {
    auto entity = m_Scene->CreateEntity("PrefabInstance");
    PixelEngine::UUID prefabID = 9876;
    entity.AddComponent<PixelEngine::PrefabComponent>(prefabID, entity.GetComponent<PixelEngine::IDComponent>().ID);

    auto& pc = entity.GetComponent<PixelEngine::PrefabComponent>();
    EXPECT_TRUE(pc.OverriddenFields.empty());

    // Track an override
    PixelEngine::TrackOverride(entity, "TransformComponent.Translation");
    ASSERT_EQ(pc.OverriddenFields.size(), 1u);
    EXPECT_EQ(pc.OverriddenFields[0], "TransformComponent.Translation");

    // Track same override (should not duplicate)
    PixelEngine::TrackOverride(entity, "TransformComponent.Translation");
    EXPECT_EQ(pc.OverriddenFields.size(), 1u);
}

TEST_F(TestEditorSuite, EntityValidityCheck) {
    auto entity = m_Scene->CreateEntity("TestValidity");
    EXPECT_TRUE(entity); // Should be true initially

    m_Scene->DestroyEntity(entity);
    EXPECT_FALSE(entity); // Should now be false because it is destroyed in the registry
}

TEST_F(TestEditorSuite, EntityValidityCheckSceneClear) {
    auto entity = m_Scene->CreateEntity("TestValidity2");
    EXPECT_TRUE(entity);

    m_Scene->Reg().clear();
    EXPECT_FALSE(entity); // Should now be false because the registry was cleared
}
